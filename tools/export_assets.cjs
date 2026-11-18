'use strict';
// Export board assets using a separately checked-out PC renderer.
const fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {spawn}=require('node:child_process');
const {createRequire}=require('node:module');
const {parseArgs}=require('node:util');
const {createInterface}=require('node:readline');
const root=path.resolve(__dirname,'..'),sleep=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
 const {values}=parseArgs({options:{'pc-root':{type:'string'},python:{type:'string',default:'python3'},'output-dir':{type:'string',default:root},check:{type:'boolean',default:false},help:{type:'boolean',default:false}}});
 if(values.help){console.log('用法：node tools/export_assets.cjs --pc-root /path/to/electronic_pet_PC [--python python3] [--output-dir DIR] [--check]\n需要独立 PC 项目 packaging/ 中的 npm 依赖和本机 Chrome。--check 只验证渲染器，不覆盖素材。');return;}
 if(!values['pc-root'])throw Error('请通过 --pc-root 指定独立 PC 项目根目录，见 docs/开发说明.md');
 const pc=path.resolve(values['pc-root'],'packaging'),backend=path.join(pc,'backend');
 if(!fs.existsSync(path.join(backend,'app.py')))throw Error('PC 项目缺少 packaging/backend/app.py');
 const {chromium}=createRequire(path.join(pc,'package.json'))('playwright');
 const temp=fs.mkdtempSync(path.join(os.tmpdir(),'lanluo-asset-export-'));
 let py,browser;
 try{
  py=spawn(values.python,['-B','-u',path.join(backend,'app.py'),'--hosted','--port','0','--no-browser','--data-dir',path.join(temp,'profile')],{cwd:backend,stdio:['pipe','pipe','pipe']});
  let errors='';py.stderr.on('data',chunk=>{errors=(errors+chunk).slice(-2000);});
  const ready=await new Promise((resolve,reject)=>{
   const lines=createInterface({input:py.stdout});
   const timer=setTimeout(()=>reject(Error('PC 导出服务启动超时：'+errors)),20000);
   py.once('error',e=>{clearTimeout(timer);reject(e);});
   py.once('exit',code=>{clearTimeout(timer);reject(Error('PC 导出服务退出：'+code+' '+errors));});
   lines.on('line',line=>{if(line.startsWith('LANLUO_READY ')){clearTimeout(timer);try{resolve(JSON.parse(line.slice(13)));}catch(e){reject(e);}}});
  });
  const origin=new URL(ready.url);if(!['127.0.0.1','localhost'].includes(origin.hostname))throw Error('渲染服务必须在本机运行');
  browser=await chromium.launch({channel:'chrome',headless:true,args:process.platform==='darwin'?['--use-angle=metal']:[]});
  const page=await browser.newPage({viewport:{width:320,height:320},deviceScaleFactor:1});
  await page.goto(origin.origin+'/?desktop=1&board=1');
  await page.waitForFunction(()=>window.petDiagnostics?.().ready&&window.petDiagnostics?.().danceDuration&&typeof window.boardJPEG==='function',{}, {timeout:60000});
  if(values.check){const jpg=Buffer.from(await page.evaluate(()=>window.boardJPEG()),'base64');if(jpg[0]!==255||jpg[1]!==216)throw Error('渲染器未返回 JPEG');console.log('RENDERER_CHECK_PASS: 320x320 board renderer, '+jpg.length+' JPEG bytes');return;}
  const config=await(await fetch(origin.origin+'/api/config')).json();
  async function cmd(data){const r=await fetch(origin.origin+'/api/command',{method:'POST',headers:{'Content-Type':'application/json','X-Pet-Token':config.token},body:JSON.stringify(data)});if(!r.ok)throw Error(await r.text());}
  const specs=[['idle',4],['wave',3],['blink',.5],['wink',1.2],['smile',2.8],['nod',2],['shake',2.2],['dance',4.5],['jump',2],['grace',24]];
  const frames=[],clips=[],previews=[];let offset=0;
  for(const [name,duration] of specs){
   await cmd({type:'reset'});await cmd({type:'settings',speed:1,loop:false});
   if(name!=='idle')await cmd({type:'action',name});
   const first=frames.length,count=Math.ceil(duration*8),started=Date.now();
   for(let i=0;i<count;i++){
    await sleep(Math.max(0,started+i*125-Date.now()));
    const jpg=Buffer.from(await page.evaluate(()=>window.boardJPEG()),'base64');
    if(i===0)previews.push({name,jpg});
    frames.push({offset,length:jpg.length,jpg});offset+=jpg.length;
   }
   clips.push({name,first,count});console.log(name,count,'frames',offset,'bytes');
  }
  const output=path.resolve(values['output-dir']);fs.mkdirSync(path.join(output,'assets'),{recursive:true});fs.mkdirSync(path.join(output,'main'),{recursive:true});
  for(const {name,jpg} of previews)fs.writeFileSync(path.join(output,'assets',name+'.jpg'),jpg);
  fs.writeFileSync(path.join(output,'assets/pet_frames.bin'),Buffer.concat(frames.map(x=>x.jpg)));
  fs.writeFileSync(path.join(output,'main/pet_assets.h'),`#pragma once\n#include <stdint.h>\ntypedef struct { uint32_t offset, length; } pet_frame_t;\ntypedef struct { const char *name; uint16_t first, count; } pet_clip_t;\nstatic const pet_frame_t pet_frames[]={\n${frames.map(x=>`{${x.offset},${x.length}}`).join(',\n')}\n};\nstatic const pet_clip_t pet_clips[]={\n${clips.map(x=>`{"${x.name}",${x.first},${x.count}}`).join(',\n')}\n};\n#define PET_CLIP_COUNT ${clips.length}\n`);
  fs.writeFileSync(path.join(output,'assets/manifest.json'),JSON.stringify({fps:8,width:320,height:320,totalBytes:offset,clips},null,2)+'\n');
 }finally{try{await browser?.close();}finally{py?.stdin.end();py?.kill();fs.rmSync(temp,{recursive:true,force:true});}}
})().catch(e=>{console.error(e.message);process.exitCode=1;});
