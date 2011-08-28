// generated by Fast Light User Interface Designer (fluid) version 1.00

#ifndef xfsmmixer_h
#define xfsmmixer_h
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
extern Fl_Window *mixer_ad1848;
#include <FL/Fl_Box.H>
#include <FL/Fl_Value_Slider.H>
extern void update_ad1848(Fl_Widget*, void*);
extern void update_ad1848(Fl_Value_Slider*, void*);
extern Fl_Value_Slider *ad1848_inl;
extern Fl_Value_Slider *ad1848_inr;
extern Fl_Value_Slider *ad1848_outl;
extern Fl_Value_Slider *ad1848_outr;
#include <FL/Fl_Button.H>
extern void cb_quit(Fl_Button*, long);
extern Fl_Button *ad1848_quit;
#include <FL/Fl_Group.H>
extern Fl_Group *ad1848_srcr;
#include <FL/Fl_Check_Button.H>
extern void update_ad1848(Fl_Check_Button*, void*);
extern Fl_Check_Button *ad1848_srcr_line;
extern Fl_Check_Button *ad1848_srcr_aux1;
extern Fl_Check_Button *ad1848_srcr_mic;
extern Fl_Check_Button *ad1848_srcr_dac;
extern Fl_Group *ad1848_srcl;
extern Fl_Check_Button *ad1848_srcl_line;
extern Fl_Check_Button *ad1848_srcl_aux1;
extern Fl_Check_Button *ad1848_srcl_mic;
extern Fl_Check_Button *ad1848_srcl_dac;
Fl_Window* create_form_ad1848();
extern Fl_Window *mixer_ct1335;
extern void update_ct1335(Fl_Widget*, void*);
extern void update_ct1335(Fl_Value_Slider*, void*);
extern Fl_Value_Slider *ct1335_out;
extern Fl_Button *ct1335_quit;
Fl_Window* create_form_ct1335();
extern Fl_Window *mixer_ct1345;
extern void update_ct1345(Fl_Widget*, void*);
extern void update_ct1345(Fl_Value_Slider*, void*);
extern Fl_Value_Slider *ct1345_outl;
extern Fl_Value_Slider *ct1345_outr;
extern Fl_Button *ct1345_quit;
extern Fl_Group *ct1345_src;
extern void update_ct1345(Fl_Check_Button*, void*);
extern Fl_Check_Button *ct1345_src_mic;
extern Fl_Check_Button *ct1345_src_cd;
extern Fl_Check_Button *ct1345_src_line;
Fl_Window* create_form_ct1345();
extern Fl_Window *mixer_ct1745;
extern void update_ct1745(Fl_Widget*, void*);
extern void update_ct1745(Fl_Value_Slider*, void*);
extern Fl_Value_Slider *ct1745_inl;
extern Fl_Value_Slider *ct1745_inr;
extern Fl_Value_Slider *ct1745_outl;
extern Fl_Value_Slider *ct1745_outr;
extern Fl_Button *ct1745_quit;
extern Fl_Value_Slider *ct1745_treble;
extern Fl_Value_Slider *ct1745_bass;
extern Fl_Group *ct1745_srcl;
extern void update_ct1745(Fl_Check_Button*, void*);
extern Fl_Check_Button *ct1745_srcl_mic;
extern Fl_Check_Button *ct1745_srcl_cdl;
extern Fl_Check_Button *ct1745_srcl_cdr;
extern Fl_Check_Button *ct1745_srcl_linel;
extern Fl_Check_Button *ct1745_srcl_midil;
extern Fl_Check_Button *ct1745_srcl_midir;
extern Fl_Check_Button *ct1745_srcl_liner;
extern Fl_Group *ct1745_srcr;
extern Fl_Check_Button *ct1745_srcr_mic;
extern Fl_Check_Button *ct1745_srcr_cdl;
extern Fl_Check_Button *ct1745_srcr_cdr;
extern Fl_Check_Button *ct1745_srcr_linel;
extern Fl_Check_Button *ct1745_srcr_midil;
extern Fl_Check_Button *ct1745_srcr_midir;
extern Fl_Check_Button *ct1745_srcr_liner;
Fl_Window* create_form_ct1745();
#endif