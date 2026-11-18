'use strict';
const {spawn}=require('node:child_process');
const fs=require('node:fs/promises'),path=require('node:path'),os=require('node:os');
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
class BoardBridge {
 constructor({root,onAction,onConnection,onVoice}){
  Object.assign(this,{root,onAction,onConnection,onVoice});
  this.sequence=0;this.pending=new Map();this.frames=0;this.connected=false;
  this.closed=false;this.speechGeneration=0;this.voiceChild=null;this.worker=null;
 }
 start(){
  this.worker=spawn(path.join(this.root,'python/bin/python'),['-B','-u',path.join(this.root,'serial_worker.py')],{stdio:['pipe','pipe','pipe']});
  let buffer='';
  this.worker.stdout.on('data',chunk=>{
   buffer+=chunk;for(;;){const at=buffer.indexOf('\n');if(at<0)break;const line=buffer.slice(0,at);buffer=buffer.slice(at+1);
    let event;try{event=JSON.parse(line);}catch{continue;}
    if(event.type==='result'){const request=this.pending.get(event.id);if(request){clearTimeout(request.timer);this.pending.delete(event.id);request.resolve(event);}}
    else if(event.type==='event')this.onAction?.(event.action);
    else if(event.type==='connection'){this.connected=event.connected;this.onConnection?.(event);}
    else if(event.type==='device')this.device=event;
   }
  });
  this.worker.stderr.on('data',chunk=>console.error('BOARD_WORKER',String(chunk).slice(0,500)));
  this.worker.stdin.on('error',()=>{}); // A disconnected helper is handled by exit and pending timeouts.
  this.worker.on('error',error=>this.onConnection?.({connected:false,message:error.message}));
  this.worker.on('exit',()=>{this.connected=false;for(const req of this.pending.values()){clearTimeout(req.timer);req.resolve({ok:false,error:'USB worker exited'});}this.pending.clear();});
 }
 send(kind,data=Buffer.alloc(0)){
  if(this.closed||!this.worker||this.worker.stdin.destroyed)return Promise.resolve({ok:false});
  if(this.pending.size>=8)return Promise.resolve({ok:false,error:'USB queue full'});
  const id=++this.sequence;
  return new Promise(resolve=>{
   const timer=setTimeout(()=>{this.pending.delete(id);resolve({ok:false,error:'USB timeout'});},10000);
   this.pending.set(id,{resolve,timer});
   this.worker.stdin.write(JSON.stringify({id,kind,data:Buffer.from(data).toString('base64')})+'\n',error=>{if(error){clearTimeout(timer);this.pending.delete(id);resolve({ok:false});}});
  });
 }
 async stream(window){
  this.window=window;
  while(!this.closed&&!window.isDestroyed()){
   const started=Date.now();
   try{
    const ready=await window.webContents.executeJavaScript('!!window.petDiagnostics?.().ready');
    if(ready){
     const picture=await window.webContents.capturePage();
     const jpeg=picture.resize({width:320,height:320,quality:'good'}).toJPEG(76);
     const result=await this.send(1,jpeg);
     if(result.ok)this.frames++;
    }
   }catch(error){if(!this.closed)console.error('BOARD_FRAME',error.message);}
   await sleep(Math.max(20,100-(Date.now()-started)));
  }
 }
 cancelSpeech(){this.speechGeneration++;this.voiceChild?.kill();this.voiceChild=null;this.onVoice?.(false);void this.send(3,Buffer.from('stop_audio'));}
 run(file,args){return new Promise((resolve,reject)=>{
  const child=spawn(file,args,{stdio:['ignore','ignore','pipe']});this.voiceChild=child;let message='';
  const timeout=setTimeout(()=>child.kill(),90000);
  child.stderr.on('data',data=>{message+=data;});
  child.on('error',error=>{clearTimeout(timeout);reject(error);});
  child.on('exit',code=>{clearTimeout(timeout);if(this.voiceChild===child)this.voiceChild=null;code===0?resolve():reject(new Error(message.slice(0,200)||'Speech cancelled'));});
 });}
 async speak(text){
  this.cancelSpeech();const generation=this.speechGeneration;
  if(!this.connected)throw new Error('请连接开发板后再朗读。');
  if(typeof text!=='string'||text.length>1600)throw new Error('朗读文字过长。');
  const temp=await fs.mkdtemp(path.join(os.tmpdir(),'lanluo-board-'));
  try{
   await fs.writeFile(path.join(temp,'text.txt'),text);
   await this.run('/usr/bin/say',['-v','Tingting','-r','165','-f',path.join(temp,'text.txt'),'-o',path.join(temp,'voice.aiff')]);
   if(generation!==this.speechGeneration)return;
   await this.run('/opt/homebrew/bin/ffmpeg',['-v','error','-y','-i',path.join(temp,'voice.aiff'),'-f','s16le','-ac','1','-ar','16000',path.join(temp,'voice.pcm')]);
   if(generation!==this.speechGeneration)return;
   const pcm=await fs.readFile(path.join(temp,'voice.pcm'));
   if(pcm.length>4*1024*1024)throw new Error('语音太长，请缩短回复。');
   this.onVoice?.(true);
   for(let at=0;at<pcm.length;at+=6400){
    if(generation!==this.speechGeneration)return;
    const result=await this.send(2,pcm.subarray(at,at+6400));
    if(!result.ok)throw new Error('开发板音频连接中断。');
   }
   await this.send(4);
   await sleep(1800); // Drain the bounded on-device audio queue.
  }finally{
   if(generation===this.speechGeneration)this.onVoice?.(false);
   await fs.rm(temp,{recursive:true,force:true});
  }
 }
 close(){this.closed=true;this.speechGeneration++;this.voiceChild?.kill();this.worker?.stdin.end();setTimeout(()=>this.worker?.kill(),500);}
}
module.exports={BoardBridge};
