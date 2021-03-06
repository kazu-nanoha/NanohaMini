/*
  NanohaMini, a USI shogi(japanese-chess) playing engine derived from Stockfish 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2010 Marco Costalba, Joona Kiiski, Tord Romstad (Stockfish author)
  Copyright (C) 2014-2016 Kazuyuki Kawabata

  NanohaMini is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  NanohaMini is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if !defined(MISC_H_INCLUDED)
#define MISC_H_INCLUDED

#include <string>
#include "types.h"

extern const std::string engine_name();
extern const std::string engine_authors();
extern int get_system_time();
extern int cpu_count();
extern int input_available();
extern void prefetch(char* addr);

extern void dbg_hit_on(bool b);
extern void dbg_hit_on_c(bool c, bool b);
extern void dbg_before();
extern void dbg_after();
extern void dbg_mean_of(int v);
extern void dbg_print_hit_rate();
extern void dbg_print_mean();

#endif // !defined(MISC_H_INCLUDED)
