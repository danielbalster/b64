/*
 * Balster64, hacking a C64 emulator into an ESP32 microcontroller
 *
 * Copyright (C) Daniel Balster
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Daniel Balster nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL BALSTER ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL DANIEL BALSTER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "note.h"

#include <algorithm>

static const NoteEntry notes[96] = {
  {"c-0",278,268},
  {"c#0",295,284},
  {"d-0",313,301},
  {"d#0",331,319},
  {"e-0",351,338},
  {"f-0",372,358},
  {"f#0",394,379},
  {"g-0",417,402},
  {"g#0",442,426},
  {"a-0",468,451},
  {"a#0",496,478},
  {"b-0",526,506},
  {"c-1",557,536},
  {"c#1",590,568},
  {"d-1",625,602},
  {"d#1",662,638},
  {"e-1",702,676},
  {"f-1",743,716},
  {"f#1",788,759},
  {"g-1",834,804},
  {"g#1",884,852},
  {"a-1",937,902},
  {"a#1",992,956},
  {"b-1",1051,1013},
  {"c-2",1114,1073},
  {"c#2",1180,1137},
  {"d-2",1250,1204},
  {"d#2",1325,1276},
  {"e-2",1403,1352},
  {"f-2",1487,1432},
  {"f#2",1575,1517},
  {"g-2",1669,1608},
  {"g#2",1768,1703},
  {"a-2",1873,1804},
  {"a#2",1985,1912},
  {"b-2",2103,2025},
  {"c-3",2228,2146},
  {"c#3",2360,2274},
  {"d-3",2500,2409},
  {"d#3",2649,2552},
  {"e-3",2807,2704},
  {"f-3",2973,2864},
  {"f#3",3150,3035},
  {"g-3",3338,3215},
  {"g#3",3536,3406},
  {"a-3",3746,3609},
  {"a#3",3969,3824},
  {"b-3",4205,4051},
  {"c-4",4455,4292},
  {"c#4",4720,4547},
  {"d-4",5001,4817},
  {"d#4",5298,5104},
  {"e-4",5613,5407},
  {"f-4",5947,5729},
  {"f#4",6300,6070},
  {"g-4",6675,6430},
  {"g#4",7072,6813},
  {"a-4",7493,7218},
  {"a#4",7938,7647},
  {"b-4",8410,8102},
  {"c-5",8910,8584},
  {"c#5",9440,9094},
  {"d-5",10001,9635},
  {"d#5",10596,10208},
  {"e-5",11226,10815},
  {"f-5",11894,11458},
  {"f#5",12601,12139},
  {"g-5",13350,12861},
  {"g#5",14144,13626},
  {"a-5",14985,14436},
  {"a#5",15876,15294},
  {"b-5",16820,16204},
  {"c-6",17820,17167},
  {"c#6",18880,18188},
  {"d-6",20003,19270},
  {"d#6",21192,20415},
  {"e-6",22452,21629},
  {"f-6",23787,22916},
  {"f#6",25202,24278},
  {"g-6",26700,25722},
  {"g#6",28288,27251},
  {"a-6",29970,28872},
  {"a#6",31752,30589},
  {"b-6",33640,32407},
  {"c-7",35641,34334},
  {"c#7",37760,36376},
  {"d-7",40005,38539},
  {"d#7",42384,40831},
  {"e-7",44904,43259},
  {"f-7",47574,45831},
  {"f#7",50403,48556},
  {"g-7",53401,51444},
  {"g#7",56576,54503},
  {"a-7",59940,57743},
  {"a#7",63504,61177},
  {"b-7",65535,64815},
};

static bool compare_noteentry_pal (const NoteEntry& _e, uint16_t v)
{
  return _e.pal < v;
}

const NoteEntry* findClosestNote(uint16_t value)
{
  return (const NoteEntry*) std::lower_bound (&notes[0],&notes[95],value,compare_noteentry_pal);
}
