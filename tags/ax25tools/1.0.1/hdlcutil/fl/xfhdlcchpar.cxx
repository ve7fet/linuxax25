// generated by Fast Light User Interface Designer (fluid) version 1.00

#include "xfhdlcchpar.h"

Fl_Window *chpar=(Fl_Window *)0;

Fl_Value_Slider *txdelay=(Fl_Value_Slider *)0;

Fl_Value_Slider *txtail=(Fl_Value_Slider *)0;

Fl_Value_Slider *slottime=(Fl_Value_Slider *)0;

Fl_Value_Slider *ppersist=(Fl_Value_Slider *)0;

Fl_Check_Button *fulldup=(Fl_Check_Button *)0;

Fl_Button *quit=(Fl_Button *)0;

Fl_Output *ifname=(Fl_Output *)0;

Fl_Window* create_the_forms() {
  Fl_Window* w;
  { Fl_Window* o = chpar = new Fl_Window(390, 260, "HDLC channel parameters");
    w = o;
    o->box(FL_NO_BOX);
    o->labeltype(FL_NORMAL_LABEL);
    { Fl_Box* o = new Fl_Box(0, 0, 390, 260);
      o->box(FL_UP_BOX);
    }
    { Fl_Box* o = new Fl_Box(10, 10, 370, 240);
      o->box(FL_DOWN_BOX);
    }
    { Fl_Value_Slider* o = txdelay = new Fl_Value_Slider(24, 30, 341, 30, "Tx Delay (ms)");
      o->type(3);
      o->labelsize(8);
      o->callback((Fl_Callback*)cb_update);
    }
    { Fl_Value_Slider* o = txtail = new Fl_Value_Slider(24, 80, 341, 30, "Tx Tail (ms)");
      o->type(3);
      o->labelsize(8);
      o->callback((Fl_Callback*)cb_update);
    }
    { Fl_Value_Slider* o = slottime = new Fl_Value_Slider(24, 130, 341, 30, "Slottime (ms)");
      o->type(3);
      o->labelsize(8);
      o->callback((Fl_Callback*)cb_update);
    }
    { Fl_Value_Slider* o = ppersist = new Fl_Value_Slider(24, 180, 341, 30, "P-Persistence");
      o->type(3);
      o->labelsize(8);
      o->callback((Fl_Callback*)cb_update);
    }
    { Fl_Check_Button* o = fulldup = new Fl_Check_Button(20, 220, 90, 20, "Full Duplex");
      o->down_box(FL_DIAMOND_DOWN_BOX);
      o->selection_color(3);
      o->callback((Fl_Callback*)cb_update);
    }
    { Fl_Button* o = quit = new Fl_Button(290, 220, 70, 20, "Quit");
      o->callback((Fl_Callback*)cb_quit, (void*)(0));
    }
    { Fl_Box* o = new Fl_Box(150, 220, 60, 20, "Interface");
      o->box(FL_FLAT_BOX);
      o->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    }
    { Fl_Output* o = ifname = new Fl_Output(220, 220, 60, 20);
      o->box(FL_EMBOSSED_BOX);
      o->selection_color(49);
      o->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    }
    o->end();
  }
  return w;
}