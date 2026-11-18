const fs=require('node:fs'),path=require('node:path'),{spawn}=require('node:child_process');
const {chromium}=require('../companion/node_modules/playwright');
const root=path.resolve(__dirname,'..');
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
 const py=spawn(path.join(root,'companion/python/bin/python'),['-B','-u',path.join(root,'companion/backend/app.py'),'--port','8877','--no-browser','--data-dir',path.join(root,'artifacts/export-profile')],{stdio:'ignore'});
 let browser;
 try{
  await sleep(1200);
  browser=await chromium.launch({channel:'chrome',headless:true,args:['--use-angle=metal']});
  const page=await browser.newPage({viewport:{width:320,height:320},deviceScaleFactor:1});
  await page.goto('http://127.0.0.1:8877/?desktop=1&board=1');
  await page.waitForFunction(()=>window.petDiagnostics?.().ready&&window.petDiagnostics?.().danceDuration,{},{timeout:60000});
  const config=await (await fetch('http://127.0.0.1:8877/api/config')).json();
  async function cmd(data){const r=await fetch('http://127.0.0.1:8877/api/command',{method:'POST',headers:{'Content-Type':'application/json','X-Pet-Token':config.token},body:JSON.stringify(data)});if(!r.ok)throw Error(await r.text());}
  const specs=[['idle',4],['wave',3],['blink',.5],['wink',1.2],['smile',2.8],['nod',2],['shake',2.2],['dance',4.5],['jump',2],['grace',24]];
  const frames=[],clips=[];let offset=0;
  fs.mkdirSync(path.join(root,'assets'),{recursive:true});
  for(const [name,duration] of specs){
   await cmd({type:'reset'});await cmd({type:'settings',speed:1,loop:false});
   if(name!=='idle')await cmd({type:'action',name});
   const first=frames.length,count=Math.ceil(duration*8),started=Date.now();
   for(let i=0;i<count;i++){
    await sleep(Math.max(0,started+i*125-Date.now()));
    const jpg=Buffer.from(await page.evaluate(()=>window.boardJPEG()),'base64');
    if(i===0)fs.writeFileSync(path.join(root,'assets',name+'.jpg'),jpg);
    frames.push({offset,length:jpg.length,jpg});offset+=jpg.length;
   }
   clips.push({name,first,count});console.log(name,count,'frames',offset,'bytes');
  }
  fs.writeFileSync(path.join(root,'assets/pet_frames.bin'),Buffer.concat(frames.map(x=>x.jpg)));
  fs.writeFileSync(path.join(root,'main/pet_assets.h'),`#pragma once\n#include <stdint.h>\ntypedef struct { uint32_t offset, length; } pet_frame_t;\ntypedef struct { const char *name; uint16_t first, count; } pet_clip_t;\nstatic const pet_frame_t pet_frames[]={\n${frames.map(x=>`{${x.offset},${x.length}}`).join(',\n')}\n};\nstatic const pet_clip_t pet_clips[]={\n${clips.map(x=>`{"${x.name}",${x.first},${x.count}}`).join(',\n')}\n};\n#define PET_CLIP_COUNT ${clips.length}\n`);
  fs.writeFileSync(path.join(root,'assets/manifest.json'),JSON.stringify({fps:8,width:320,height:320,totalBytes:offset,clips},null,2));
 } finally {await browser?.close();py.kill();}
})().catch(e=>{console.error(e);process.exitCode=1;});
