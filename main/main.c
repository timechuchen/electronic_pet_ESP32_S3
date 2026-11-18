#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_rom_crc.h"
#include "esp_jpeg_dec.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "pet_assets.h"
#include "ui_text.h"

LV_FONT_DECLARE(pet_font_zh_20);
LV_FONT_DECLARE(pet_font_zh_28);

#define SIDE 320
#define MAX_PACKET 98304
static const char *TAG="pet";
extern const uint8_t assets_start[] asm("_binary_pet_frames_bin_start");
static lv_obj_t *portrait, *status_text, *caption, *btn[3], *btn_text[3];
static lv_image_dsc_t image_desc;
static uint8_t *pixels;
static jpeg_dec_handle_t decoder;
static int clip, frame, page;
static bool paused;
static int64_t last_live_us;
static uint32_t live_frames, decoded_frames, events;
typedef struct { uint8_t *bytes; uint32_t size; bool end; } sound_packet_t;
static QueueHandle_t sound_queue;
static bool sound_available;
static const char *pages[5][3]={{"wave","dance","more"},{"smile","wink","more"},{"nod","shake","more"},{"jump","grace","more"},{"blink","pause","home"}};
static const char *page_labels[5][3]=UI_PAGE_LABELS;

// Call only while holding the BSP/LVGL lock. JPEGs are strictly bounded.
static bool decode_frame(const uint8_t *jpeg, uint32_t size)
{
    jpeg_dec_io_t io={.inbuf=(uint8_t *)jpeg,.inbuf_len=size,.outbuf=pixels};
    jpeg_dec_header_info_t info;
    if(jpeg_dec_parse_header(decoder,&io,&info)!=JPEG_ERR_OK || info.width!=SIDE || info.height!=SIDE) return false;
    int needed=0;
    if(jpeg_dec_get_outbuf_len(decoder,&needed)!=JPEG_ERR_OK || needed>SIDE*SIDE*2) return false;
    if(jpeg_dec_process(decoder,&io)!=JPEG_ERR_OK) return false;
    lv_image_cache_drop(&image_desc);
    lv_obj_invalidate(portrait);
    decoded_frames++;
    return true;
}

static bool connected(void){return last_live_us && esp_timer_get_time()-last_live_us<2500000;}
static void refresh_button_labels(void)
{
    for(int i=0;i<3;i++)lv_label_set_text(btn_text[i],strcmp(pages[page][i],"pause")==0?(paused?UI_CONTINUE:UI_PAUSE):page_labels[page][i]);
}
static const char *action_label(const char *name)
{
    for(int p=0;p<5;p++)for(int i=0;i<3;i++)if(strcmp(pages[p][i],name)==0)return page_labels[p][i];
    return UI_IDLE;
}
static void choose_action(const char *name, bool notify)
{
    if(strcmp(name,"more")==0 || strcmp(name,"home")==0){
        page=strcmp(name,"home")==0?0:(page+1)%5;
        refresh_button_labels();
        return;
    }
    if(strcmp(name,"pause")==0)paused=!paused;
    else if(strcmp(name,"reset")==0){clip=0;frame=0;paused=false;}
    else if(strcmp(name,"chat")!=0){
        for(int i=0;i<PET_CLIP_COUNT;i++)if(strcmp(pet_clips[i].name,name)==0){clip=i;frame=0;paused=false;break;}
    }
    lv_label_set_text(caption,paused?UI_REST:strcmp(name,"chat")==0?(connected()?UI_CHAT_LIVE:UI_CHAT_OFFLINE):strcmp(name,"pause")==0?UI_RESUME:action_label(name));
    refresh_button_labels();
    if(notify){events++;printf("PET_EVENT {\"action\":\"%s\",\"number\":%" PRIu32 "}\n",name,events);fflush(stdout);}
}

static void button_event(lv_event_t *e){choose_action(pages[page][(intptr_t)lv_event_get_user_data(e)],true);}
static void pet_touch(lv_event_t *e){choose_action(lv_event_get_code(e)==LV_EVENT_LONG_PRESSED?"reset":"smile",true);}
static void chat_touch(lv_event_t *e){(void)e;choose_action("chat",true);}
static void animate(lv_timer_t *timer)
{
    (void)timer;
    bool live=connected();
    lv_label_set_text(status_text,live?UI_LIVE:UI_OFFLINE);
    lv_obj_set_style_text_color(status_text,lv_color_hex(live?0x79E4BD:0xAFBDC9),0);
    if(!live && !paused){
        const pet_clip_t *c=&pet_clips[clip];
        const pet_frame_t *f=&pet_frames[c->first+frame];
        decode_frame(assets_start+f->offset,f->length);
        if(++frame>=c->count){frame=0;if(clip!=0){clip=0;lv_label_set_text(caption,UI_IDLE);}}
    }
}

static lv_obj_t *text(lv_obj_t *parent,const char *s,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *l=lv_label_create(parent);lv_label_set_text(l,s);
    lv_obj_set_style_text_font(l,font,0);lv_obj_set_style_text_color(l,lv_color_hex(color),0);return l;
}
static void ui_init(void)
{
    lv_obj_t *screen=lv_screen_active();lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen,lv_color_black(),0);
    lv_obj_t *name=text(screen,UI_TITLE,&pet_font_zh_28,0xF0F6F2);
    lv_obj_set_style_text_letter_space(name,4,0);
    lv_obj_align(name,LV_ALIGN_TOP_MID,0,27);
    status_text=text(screen,UI_OFFLINE,&pet_font_zh_20,0xAFBDC9);
    lv_obj_align(status_text,LV_ALIGN_TOP_MID,0,60);
    lv_obj_set_ext_click_area(status_text,14);lv_obj_add_flag(status_text,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_text,chat_touch,LV_EVENT_CLICKED,NULL);
    image_desc=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=SIDE,.h=SIDE,.stride=SIDE*2},.data_size=SIDE*SIDE*2,.data=pixels};
    portrait=lv_image_create(screen);lv_image_set_src(portrait,&image_desc);lv_obj_set_pos(portrait,89,85);
    lv_image_set_pivot(portrait,0,0);lv_image_set_scale(portrait,230);
    lv_obj_add_flag(portrait,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(portrait,pet_touch,LV_EVENT_SHORT_CLICKED,NULL);
    lv_obj_add_event_cb(portrait,pet_touch,LV_EVENT_LONG_PRESSED,NULL);
    caption=text(screen,UI_IDLE,&pet_font_zh_20,0xAFBDC9);lv_obj_align(caption,LV_ALIGN_TOP_MID,0,344);
    for(int i=0;i<3;i++){
        btn[i]=lv_button_create(screen);lv_obj_set_pos(btn[i],94+i*94,374);lv_obj_set_size(btn[i],90,46);
        lv_obj_set_style_radius(btn[i],18,0);lv_obj_set_style_bg_color(btn[i],lv_color_hex(i==0?0x79E4BD:0x20313C),0);
        lv_obj_set_style_shadow_width(btn[i],0,0);lv_obj_set_style_pad_all(btn[i],0,0);
        lv_obj_set_style_bg_color(btn[i],lv_color_hex(0x456F68),LV_STATE_PRESSED);
        btn_text[i]=text(btn[i],page_labels[0][i],&pet_font_zh_20,i==0?0x08141B:0xE9F2EF);lv_obj_center(btn_text[i]);
        lv_obj_add_event_cb(btn[i],button_event,LV_EVENT_CLICKED,(void *)(intptr_t)i);
    }
    decode_frame(assets_start+pet_frames[0].offset,pet_frames[0].length);
    lv_timer_create(animate,125,NULL);
}

static void sound_task(void *arg)
{
    (void)arg;
    esp_codec_dev_handle_t speaker=bsp_audio_codec_speaker_init();
    sound_available=speaker!=NULL;
    bool opened=false;
    esp_codec_dev_sample_info_t format={.sample_rate=16000,.channel=1,.bits_per_sample=16};
    sound_packet_t pkt;
    for(;;){
        if(xQueueReceive(sound_queue,&pkt,portMAX_DELAY)!=pdTRUE)continue;
        if(pkt.end){if(opened)esp_codec_dev_close(speaker);opened=false;}
        else if(speaker){
            if(!opened){opened=esp_codec_dev_open(speaker,&format)==ESP_OK;if(opened)esp_codec_dev_set_out_vol(speaker,55);}
            if(opened)esp_codec_dev_write(speaker,pkt.bytes,pkt.size);
        }
        free(pkt.bytes);
    }
}

static bool read_exact(uint8_t *buf,size_t len)
{
    size_t at=0;int64_t deadline=esp_timer_get_time()+2000000;
    while(at<len){int n=usb_serial_jtag_read_bytes(buf+at,len-at,pdMS_TO_TICKS(100));if(n>0)at+=n;
        else if(esp_timer_get_time()>deadline)return false;}
    return true;
}
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void ack(uint32_t seq,bool ok){printf("PET_ACK {\"seq\":%" PRIu32 ",\"ok\":%s,\"frames\":%" PRIu32 "}\n",seq,ok?"true":"false",live_frames);fflush(stdout);}

static void snapshot(void)
{
    lv_draw_buf_t *b=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_RGB565);if(!b)return;
    printf("PET_SNAPSHOT %u %u %u %u\n",(unsigned)b->header.w,(unsigned)b->header.h,(unsigned)b->header.stride,(unsigned)b->data_size);fflush(stdout);
    size_t pos=0;while(pos<b->data_size){size_t chunk=b->data_size-pos;if(chunk>1024)chunk=1024;int n=usb_serial_jtag_write_bytes(b->data+pos,chunk,pdMS_TO_TICKS(1000));if(n<=0)break;pos+=n;}
    lv_draw_buf_destroy(b);
}
static void usb_task(void *arg)
{
    (void)arg;
    uint8_t *payload=heap_caps_malloc(MAX_PACKET+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);configASSERT(payload);
    uint8_t head[16],crc[4];unsigned match=0;
    for(;;){
        uint8_t ch;if(usb_serial_jtag_read_bytes(&ch,1,pdMS_TO_TICKS(100))!=1)continue;
        if(ch==(uint8_t)"LPET"[match]){if(++match<4)continue;}else{match=ch=='L'?1:0;continue;}
        match=0;memcpy(head,"LPET",4);if(!read_exact(head+4,12))continue;
        uint32_t kind=u32(head+4),seq=u32(head+8),len=u32(head+12);
        if(len>MAX_PACKET){ack(seq,false);continue;}
        if(!read_exact(payload,len)||!read_exact(crc,4))continue;
        if(esp_rom_crc32_le(0,payload,len)!=u32(crc)){ack(seq,false);continue;}
        payload[len]=0;bool ok=false;
        if(kind==1){
            if(bsp_display_lock(1000)==ESP_OK){ok=decode_frame(payload,len);if(ok){last_live_us=esp_timer_get_time();live_frames++;}bsp_display_unlock();}
        }else if(kind==2 && len<=8192 && len%2==0){
            sound_packet_t pkt={.bytes=malloc(len),.size=len};
            if(pkt.bytes){memcpy(pkt.bytes,payload,len);ok=xQueueSend(sound_queue,&pkt,pdMS_TO_TICKS(1000))==pdTRUE;if(!ok)free(pkt.bytes);}
        }else if(kind==4){sound_packet_t pkt={.end=true};ok=xQueueSend(sound_queue,&pkt,pdMS_TO_TICKS(1000))==pdTRUE;
        }else if(kind==3 && len<64){
            if(bsp_display_lock(1000)==ESP_OK){
                if(strcmp((char *)payload,"status")==0){
                    printf("PET_STATUS {\"live\":%s,\"frames\":%" PRIu32 ",\"decoded\":%" PRIu32 ",\"action\":\"%s\",\"paused\":%s,\"sound\":%s}\n",connected()?"true":"false",live_frames,decoded_frames,pet_clips[clip].name,paused?"true":"false",sound_available?"true":"false");
                }else if(strcmp((char *)payload,"snapshot")==0)snapshot();
                else if(strcmp((char *)payload,"offline")==0){last_live_us=0;choose_action("reset",false);}
                else if(strcmp((char *)payload,"stop_audio")==0){sound_packet_t old;while(xQueueReceive(sound_queue,&old,0)==pdTRUE)free(old.bytes);sound_packet_t end={.end=true};xQueueSend(sound_queue,&end,0);}
                else choose_action((char *)payload,true);
                bsp_display_unlock();ok=true;
            }
        }
        ack(seq,ok);
    }
}

void app_main(void)
{
    usb_serial_jtag_driver_config_t cfg={.tx_buffer_size=4096,.rx_buffer_size=32768};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));usb_serial_jtag_vfs_use_driver();
    pixels=heap_caps_aligned_alloc(16,SIDE*SIDE*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);configASSERT(pixels);
    jpeg_dec_config_t jpg=DEFAULT_JPEG_DEC_CONFIG();jpg.output_type=JPEG_PIXEL_FORMAT_RGB565_LE;
    configASSERT(jpeg_dec_open(&jpg,&decoder)==JPEG_ERR_OK);
    configASSERT(bsp_display_start());ESP_ERROR_CHECK(bsp_display_brightness_set(65));
    ESP_ERROR_CHECK(bsp_display_lock(2000));ui_init();bsp_display_unlock();
    sound_queue=xQueueCreate(8,sizeof(sound_packet_t));configASSERT(sound_queue);
    configASSERT(xTaskCreate(sound_task,"pet_audio",4096,NULL,4,NULL)==pdPASS);
    configASSERT(xTaskCreate(usb_task,"pet_usb",8192,NULL,3,NULL)==pdPASS);
    ESP_LOGI(TAG,"PET_READY protocol=1 assets=%d JPEG=320x320",PET_CLIP_COUNT);
}
