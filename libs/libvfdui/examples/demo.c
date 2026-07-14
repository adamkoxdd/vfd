#define _POSIX_C_SOURCE 200809L
#include "vfdui/vfdui.h"
#include <stdio.h>
#include <time.h>
#include <math.h>

static void sleep_frame(void){struct timespec d={0,16000000};nanosleep(&d,NULL);}

int main(void){
    VfdTheme theme=vfd_theme_default();
    VfdWindow *w=vfd_window_create("VFD UI 0.4",1200,760,&theme);
    if(!w){fprintf(stderr,"failed to create VFD window\n");return 1;}

    VfdAnim intro,drawer,action;
    vfd_anim_init(&intro,0);vfd_anim_to(&intro,1,1.2,VFD_EASE_OUT_CUBIC);
    vfd_anim_init(&drawer,0);
    vfd_anim_init(&action,1);
    bool expanded=false;
    int action_mode=0;
    double load=.64;

    while(vfd_window_is_open(w)){
        VfdEvent e=vfd_window_poll(w);if(e.quit)vfd_window_close(w);
        double dt=vfd_window_delta(w),t=vfd_window_time(w);
        double intro_v=vfd_anim_update(&intro,dt);
        double drawer_v=vfd_anim_update(&drawer,dt);
        double action_v=vfd_anim_update(&action,dt);

        vfd_window_begin(w);
        int ww=vfd_window_width(w),hh=vfd_window_height(w);
        VfdRect root={20,20,ww-40,hh-40};
        vfd_panel(w,root,true);

        VfdRect content=vfd_inset(root,26);
        VfdRect header=vfd_rect_split_top(&content,88,18);
        vfd_title(w,"VFD UI ENGINE // 0.4",(VfdRect){header.x,header.y,header.width,32});
        vfd_label(w,"PHASE 3 ANIMATION + PHASE 4 LAYOUT",(VfdRect){header.x,header.y+40,header.width,24},12,false,VFD_ALIGN_LEFT,theme.dim);

        VfdLayout grid=vfd_layout_grid(content,6,3,16,0);
        for(int i=0;i<6;i++){
            VfdRect card=vfd_layout_next(&grid);
            double pulse=vfd_pulse(t+i*.17,.22,.07,.20);
            double action_x=action_mode==2?(1-action_v)*70:0;
            double action_scale=action_mode==3
                ? 1+sin(action_v*3.141592653589793)*.08
                : 1;
            double action_opacity=action_mode==1?action_v:1;

            VfdFx fx={
                .opacity=action_opacity,
                .translate_x=action_x,
                .translate_y=(1-intro_v)*24,
                .scale=(.97+.03*intro_v)*action_scale,
                .glow=pulse
            };
            vfd_fx_push(w,fx);
            vfd_panel(w,card,true);
            char title[64],value[64];
            snprintf(title,sizeof title,"MODULE %02d",i+1);
            snprintf(value,sizeof value,"%03d%%",(int)(((load+i*.055)>1?1:(load+i*.055))*100));
            vfd_label(w,title,vfd_insets(card,16,16,16,0),13,true,VFD_ALIGN_LEFT,theme.phosphor);
            vfd_meter(w,vfd_insets(card,16,54,16,card.height-82),((load+i*.055)>1?1:(load+i*.055)),18);
            vfd_label(w,value,vfd_anchor(card,100,24,VFD_ANCHOR_BOTTOM_RIGHT,14),12,true,VFD_ALIGN_RIGHT,theme.glow);
            vfd_fx_pop(w);
        }

        VfdRect exit_rect=vfd_anchor(root,170,42,VFD_ANCHOR_BOTTOM_RIGHT,26);
        VfdRect drawer_btn=vfd_anchor(root,190,42,VFD_ANCHOR_BOTTOM_LEFT,26);
        if(vfd_button(w,drawer_btn,expanded?"CLOSE DRAWER":"OPEN DRAWER",&e)){
            expanded=!expanded;vfd_anim_to(&drawer,expanded?1:0,.35,VFD_EASE_IN_OUT_QUAD);
        }
        if(vfd_button(w,exit_rect,"EXIT SYSTEM",&e))vfd_window_close(w);

        if(drawer_v>0.001){
            VfdRect drawer_rect={root.x,root.y+root.height-170*drawer_v,root.width,170};
            vfd_panel(w,drawer_rect,true);
            VfdLayout row=vfd_layout_horizontal(vfd_inset(drawer_rect,18),4,12,0,VFD_CROSS_STRETCH);
            const char *labels[]={"FADE","SLIDE","PULSE","WARMUP"};
            for(int i=0;i<4;i++){
                VfdRect r=vfd_layout_next(&row);

                if(vfd_button(w,r,labels[i],&e)){
                    action_mode=i+1;
                    vfd_anim_init(&action,0);
                    vfd_anim_to(
                        &action,
                        1,
                        i==3?1.0:.55,
                        VFD_EASE_OUT_CUBIC
                    );
                }
            }
        }

        vfd_noise(w,.45);
        vfd_vignette(w,.28);
        vfd_crt_warmup(w,action_mode==4?action_v:intro_v);
        vfd_window_end(w);
        sleep_frame();
    }
    vfd_window_destroy(w);return 0;
}
