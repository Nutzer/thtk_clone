- Supported Games
```

  This table displays which of the main actions are
supported for each game.

Action   1 2 3 4 5 6 7 75 8 9 95 10 103 105 11 12 123 125 128 13 14 143 15 16 165
thanm x  - - - - - - y  - y y  y  y   y  -  y  y   -   y   y  y  y   y  y  y   y
thanm l  - - - - - y y  - y y  y  y   y  -  y  y   -   y   y  y  y   y  y  y   y
thdat x  y y y y y y y  n y y  y  y   y  y  y  y   y   y   y  y  y   y  y  y   -
thdat c  y y y y y y y  n y y  y  y   y  y  y  y   y   y   y  y  y   y  y  y   -
thecl d  - - - - - y y  - y y  y  y   y  -  y  y   -   y   y  y  y   y  y  y   y
thecl c  - - - - - y y  - y y  y  y   y  -  y  y   -   y   y  y  y   y  y  y   y
thmsg d  - - - - - y y  - y y  y  y   -  -  y  y   -   y   y  y  y   y  y  y   y
thmsg c  - - - - - y y  - y y  y  y   -  -  y  y   -   y   y  y  y   y  y  y   y
thstd d  - - - - - y y  - y y  y  y   -  -  y  y   -   y   y  y  y   y  y  y   y
thstd c  - - - - - y y  - y y  y  y   -  -  y  y   -   y   y  y  y   y  y  y   y
```
- New Syntax

Thanmx uses an alternative anm specification syntax to help with writing anm files from scratch by automatically assigning some information.

Currently there is no dumper for that, so here is an example:
```c
/* Basic entry syntax:
   img name "file" w*h; */
img img_background: "bg.png" 640*480;

/* You can also append additional information seperated by commata */
img {
img_fairy: "fairy.png" 256*32,
            Format: 1,
            THTX-Format: 1
};

/* Basic sprite syntax:
   img_name: sprite "sprite-name" w*h+x+y; */
img_background: sprite "bg_spr" 640*480+0+0;

/* You can also specify sprite 'arrays' */
img_fairy: sprite "fairy_idle" {
    32*32+0+0,
    32*32+32+0,
    32*32+64+0,
    32*32+32+0
};

img_fairy: sprite "fairy_toside" {
    32*32+96+0,
    32*32+128+0
};

img_fairy: sprite "fairy_side" {
    32*32+160+0,
    32*32+192+0
};

/* 'map' means assigning the scripts to an entry and a number
   Syntax is:
   img_name: map {
     number: script;
     ...
   }
 */
img_background:map {
  -1: background_main;
};

img_fairy:map {
  0: fairy_idle;
  1: fairy_left;
  2: fairy_right;
  3: fairy_fromleft;
  4: fairy_fromright;
};

/* Scripts are basically the same as subs in ECL, but without the stack and rank systems. */
sub background_main() {
  /* instruction names come from a mnemonic map */
  st_set_z(4);
  /* sprites are referenced with [sprite-name] */
  st_set_sprite([bg_spr]);
  st_set_a(0);
  st_chg_a(60, 0, 255);

  /* This creates a
     ins_63();
     ins_64(2);
     ...
     ins_63();
     construct for ECL-Gotos.
  */
  2 -> {
   loop_start:
    st_chg_a(60, 0, 100);
   60:
    st_chg_a(60, 0, 155);
  120:
    goto loop_start @ 0;
  }

  1 -> {
    st_chg_a(60, 0, 0);
   180:
    ins_1();
  }
}

sub fairy_idle() {
  st_set_z(24);
loop_start:
  /* sprite arrays are referenced with [sprite-name_index] */
  st_set_sprite([fairy_idle_0]);
 5: st_set_sprite([fairy_idle_1]);
 10: st_set_sprite([fairy_idle_2]);
 15: st_set_sprite([fairy_idle_3]);
  goto loop_start @ 0;
 ins_63();
}

sub fairy_left() {
  st_set_z(24);
  st_set_sprite([fairy_toside_0]);
5: st_set_sprite([fairy_toside_1]);
loop_start:
10: st_set_sprite([fairy_side_0]);
15: st_set_sprite([fairy_side_1]);
  goto loop_start @ 10;
  ins_63();
}

sub fairy_right() {
  st_set_z(24);
  st_set_flip();
  st_set_sprite([fairy_toside_0]);
5: st_set_sprite([fairy_toside_1]);
loop_start:
10: st_set_sprite([fairy_side_0]);
15: st_set_sprite([fairy_side_1]);
  goto loop_start @ 10;
  ins_63();
}

sub fairy_fromleft() {
  st_set_z(24);
  st_set_sprite([fairy_toside_1]);
5: st_set_sprite([fairy_toside_0]);
loop_start:
10: st_set_sprite([fairy_idle_0]);
15: st_set_sprite([fairy_idle_1]);
20: st_set_sprite([fairy_idle_2]);
25: st_set_sprite([fairy_idle_3]);
  goto loop_start @ 10;
  ins_63();
}

sub fairy_fromright() {
  st_set_z(24);
  st_set_flip();
  st_set_sprite([fairy_toside_1]);
5: st_set_sprite([fairy_toside_0]);
loop_start:
10: st_set_sprite([fairy_idle_0]);
15: st_set_sprite([fairy_idle_1]);
20: st_set_sprite([fairy_idle_2]);
25: st_set_sprite([fairy_idle_3]);
  goto loop_start @ 10;
  ins_63();
}
```
