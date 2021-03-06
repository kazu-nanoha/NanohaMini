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

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include "position.h"
#include "tt.h"
#include "book.h"
#include "ucioption.h"
#if defined(EVAL_NANO)
#include "param_nano.h"
#elif defined(EVAL_MINI)
#include "param_mini.h"
#elif defined(EVAL_APERY)
#include "param_apery.h"
#endif

///#define DEBUG_GENERATE

// x が定数のときの絶対値(プリプロセッサレベルで確定するので…)
#define ABS(x)	((x) > 0 ? (x) : -(x))

const uint32_t Hand::tbl[HI+1] = {
	0, HAND_FU_INC, HAND_KY_INC, HAND_KE_INC, HAND_GI_INC, HAND_KI_INC, HAND_KA_INC, HAND_HI_INC, 
};
#if defined(ENABLE_MYASSERT)
int debug_level;
#endif

#if !defined(NDEBUG)
#define MOVE_TRACE		// 局面に至った指し手を保持する
#endif
#if defined(MOVE_TRACE)
Move m_trace[PLY_MAX_PLUS_2];
void disp_trace(int n)
{
	for (int i = 0; i < n; i++) {
		std::cerr << i << ":" << move_to_csa(m_trace[i]) << " ";
	}
}
#endif

namespace NanohaTbl {
	// 方向の定義
	const int Direction[32] = {
		DIR00, DIR01, DIR02, DIR03, DIR04, DIR05, DIR06, DIR07,
		DIR08, DIR09, DIR10, DIR11, 0,     0,     0,     0,
		DIR00, DIR01, DIR02, DIR03, DIR04, DIR05, DIR06, DIR07,
		DIR00, DIR01, DIR02, DIR03, DIR04, DIR05, DIR06, DIR07,
	};

#if !defined(TSUMESOLVER)
	// 駒の価値
	const int KomaValue[32] = {
		 0,
		 DPawn,
		 DLance,
		 DKnight,
		 DSilver,
		 DGold,
		 DBishop,
		 DRook,
		 DKing,
		 DProPawn,
		 DProLance,
		 DProKnight,
		 DProSilver,
		 0,
		 DHorse,
		 DDragon,
		 0,
		-DPawn,
		-DLance,
		-DKnight,
		-DSilver,
		-DGold,
		-DBishop,
		-DRook,
		-DKing,
		-DProPawn,
		-DProLance,
		-DProKnight,
		-DProSilver,
		-0,
		-DHorse,
		-DDragon,
	};

	// 取られたとき(捕獲されたとき)の価値
	const int KomaValueEx[32] = {
		 0 + 0,
		 DPawn + DPawn,
		 DLance + DLance,
		 DKnight + DKnight,
		 DSilver + DSilver,
		 DGold + DGold,
		 DBishop + DBishop,
		 DRook + DRook,
		 DKing + DKing,
		 DProPawn + DPawn,
		 DProLance + DLance,
		 DProKnight + DKnight,
		 DProSilver + DSilver,
		 0 + 0,
		 DHorse + DBishop,
		 DDragon + DRook,
		 0 + 0,
		-DPawn	-DPawn,
		-DLance	-DLance,
		-DKnight	-DKnight,
		-DSilver	-DSilver,
		-DGold	-DGold,
		-DBishop	-DBishop,
		-DRook	-DRook,
		-DKing	-DKing,
		-DProPawn	-DPawn,
		-DProLance	-DLance,
		-DProKnight	-DKnight,
		-DProSilver	-DSilver,
		-0	-0,
		-DHorse	-DBishop,
		-DDragon	-DRook,
	};

	// 成る価値
	const int KomaValuePro[32] = {
		 0,
		 DProPawn - DPawn,
		 DProLance - DLance,
		 DProKnight - DKnight,
		 DProSilver - DSilver,
		 0,
		 DHorse - DBishop,
		 DDragon - DRook,
		 0,
		 0,
		 0,
		 0,
		 0,
		 0,
		 0,
		 0,
		 0,
		-(DProPawn - DPawn),
		-(DProLance - DLance),
		-(DProKnight - DKnight),
		-(DProSilver - DSilver),
		 0,
		-(DHorse - DBishop),
		-(DDragon - DRook),
		 0,
		 0,
		 0,
		 0,
		 0,
		 0,
		 0,
		 0,
	};
#endif//#if !defined(TSUMESOLVER)

	// Historyのindex変換用
	const int Piece2Index[32] = {	// 駒の種類に変換する({と、杏、圭、全}を金と同一視)
		EMP, SFU, SKY, SKE, SGI, SKI, SKA, SHI,
		SOU, SKI, SKI, SKI, SKI, EMP, SUM, SRY,
		EMP, GFU, GKY, GKE, GGI, GKI, GKA, GHI,
		GOU, GKI, GKI, GKI, GKI, EMP, GUM, GRY,
	};
};

static FILE *fp_info = stdout;
static FILE *fp_log = NULL;

int output_info(const char *fmt, ...)
{
	int ret = 0;

	va_list argp;
	va_start(argp, fmt);
	if (fp_log) vfprintf(fp_log, fmt, argp);
#if !defined(USE_USI)
	if (fp_info) {
		ret = vfprintf(fp_info, fmt, argp);
	}
#endif
	va_end(argp);

	return ret;
}
int foutput_log(FILE *fp, const char *fmt, ...)
{
	int ret = 0;

	va_list argp;
	va_start(argp, fmt);
	if (fp == stdout) {
		if (fp_log) vfprintf(fp_log, fmt, argp);
	}
	if (fp) ret = vfprintf(fp, fmt, argp);
	va_end(argp);

	return ret;
}

// 実行ファイル起動時に行う初期化.
void init_application_once()
{
	Position::init_evaluate();	// 評価ベクトルの読み込み
	Position::initMate1ply();

// 定跡ファイルの読み込み
	if (book == NULL) {
		book = new Book();
		book->open(Options["BookFile"].value<std::string>());
	}

	int from;
	int to;
	int i;
	memset(Position::DirTbl, 0, sizeof(Position::DirTbl));
	for (from = 0x11; from <= 0x99; from++) {
		if ((from & 0x0F) == 0 || (from & 0x0F) > 9) continue;
		for (i = 0; i < 8; i++) {
			int dir = NanohaTbl::Direction[i];
			to = from;
			while (1) {
				to += dir;
				if ((to & 0x0F) == 0 || (to & 0x0F) >    9) break;
				if ((to & 0xF0) == 0 || (to & 0xF0) > 0x90) break;
				Position::DirTbl[from][to] = (1 << i);
			}
		}
	}
}

// 初期化関係
void Position::init_position(const unsigned char board_ori[9][9], const int Mochigoma_ori[])
{
//	Tesu = 0;

	size_t i;
	unsigned char board[9][9];
	int Mochigoma[GOTE+HI+1] = {0};
	{
		int x, y;
		for (y = 0; y < 9; y++) {
			for (x = 0; x < 9; x++) {
				board[y][x] = board_ori[y][x];
			}
		}
		memcpy(Mochigoma, Mochigoma_ori, sizeof(Mochigoma));

		// 持ち駒設定。
		handS.set(&Mochigoma[SENTE]);
		handG.set(&Mochigoma[GOTE]);
	}

	// 盤面をWALL（壁）で埋めておきます。
	for (i = 0; i < sizeof(banpadding)/sizeof(banpadding[0]); i++) {
		banpadding[i] = WALL;
	}
	for (i = 0; i < sizeof(ban)/sizeof(ban[0]); i++) {
		ban[i] = WALL;
	}

	// 初期化
	memset(komano, 0, sizeof(komano));
	memset(knkind, 0, sizeof(knkind));
	memset(knpos,  0, sizeof(knpos));

	// boardで与えられた局面を設定します。
	int z;
	int kn;
	for(int dan = 1; dan <= 9; dan++) {
		for(int suji = 0x10; suji <= 0x90; suji += 0x10) {
			// 将棋の筋は左から右なので、配列の宣言と逆になるため、筋はひっくり返さないとなりません。
			z = suji + dan;
			ban[z] = Piece(board[dan-1][9 - suji/0x10]);

			// 駒番号系のデータ設定
#define KNABORT()	fprintf(stderr, "Error!:%s:%d:ban[0x%X] == 0x%X\n", __FILE__, __LINE__, z, ban[z]);exit(-1)
#define KNSET(kind) for (kn = KNS_##kind; kn <= KNE_##kind; kn++) {		\
						if (knkind[kn] == 0) break;		\
					}		\
					if (kn > KNE_##kind) {KNABORT();}		\
					knkind[kn] = ban[z];			\
					knpos[kn] = z;					\
					komano[z] = kn;

			switch (ban[z]) {
			case EMP:
				break;
			case SFU:
			case STO:
			case GFU:
			case GTO:
				KNSET(FU);
				break;
			case SKY:
			case SNY:
			case GKY:
			case GNY:
				KNSET(KY);
				break;
			case SKE:
			case SNK:
			case GKE:
			case GNK:
				KNSET(KE);
				break;
			case SGI:
			case SNG:
			case GGI:
			case GNG:
				KNSET(GI);
				break;
			case SKI:
			case GKI:
				KNSET(KI);
				break;
			case SKA:
			case SUM:
			case GKA:
			case GUM:
				KNSET(KA);
				break;
			case SHI:
			case SRY:
			case GHI:
			case GRY:
				KNSET(HI);
				break;
			case SOU:
				KNSET(SOU);
				break;
			case GOU:
				KNSET(GOU);
				break;
			case WALL:
			case PIECE_NONE:
			default:
				KNABORT();
				break;
			}
#undef KNABORT
#undef KNSET

		}
	}

#define KNABORT(kind)	fprintf(stderr, "Error!:%s:%d:kind=%d\n", __FILE__, __LINE__, kind);exit(-1)
#define KNHANDSET(SorG, kind) \
		for (kn = KNS_##kind; kn <= KNE_##kind; kn++) {		\
			if (knkind[kn] == 0) break;		\
		}		\
		if (kn > KNE_##kind) {KNABORT(kind);}		\
		knkind[kn] = SorG | kind;			\
		knpos[kn] = (SorG == SENTE) ? 1 : 2;

	int n;
	n = Mochigoma[SENTE+FU];	while (n--) {KNHANDSET(SENTE, FU)}
	n = Mochigoma[SENTE+KY];	while (n--) {KNHANDSET(SENTE, KY)}
	n = Mochigoma[SENTE+KE];	while (n--) {KNHANDSET(SENTE, KE)}
	n = Mochigoma[SENTE+GI];	while (n--) {KNHANDSET(SENTE, GI)}
	n = Mochigoma[SENTE+KI];	while (n--) {KNHANDSET(SENTE, KI)}
	n = Mochigoma[SENTE+KA];	while (n--) {KNHANDSET(SENTE, KA)}
	n = Mochigoma[SENTE+HI];	while (n--) {KNHANDSET(SENTE, HI)}

	n = Mochigoma[GOTE+FU];	while (n--) {KNHANDSET(GOTE, FU)}
	n = Mochigoma[GOTE+KY];	while (n--) {KNHANDSET(GOTE, KY)}
	n = Mochigoma[GOTE+KE];	while (n--) {KNHANDSET(GOTE, KE)}
	n = Mochigoma[GOTE+GI];	while (n--) {KNHANDSET(GOTE, GI)}
	n = Mochigoma[GOTE+KI];	while (n--) {KNHANDSET(GOTE, KI)}
	n = Mochigoma[GOTE+KA];	while (n--) {KNHANDSET(GOTE, KA)}
	n = Mochigoma[GOTE+HI];	while (n--) {KNHANDSET(GOTE, HI)}
#undef KNABORT
#undef KNHANDSET

	// effectB/effectWの初期化
	init_effect();

	// ピン情報の初期化
	make_pin_info();
}

// ピンの状態を設定する
void Position::make_pin_info()
{
	memset(pin+0x11, 0, (0x99-0x11+1)*sizeof(pin[0]));
	int p;
#define ADDKING1(SG, dir) \
	do {				\
		if (ban[p -= DIR_ ## dir] != EMP) break;				\
		if (ban[p -= DIR_ ## dir] != EMP) break;				\
		if (ban[p -= DIR_ ## dir] != EMP) break;				\
		if (ban[p -= DIR_ ## dir] != EMP) break;				\
		if (ban[p -= DIR_ ## dir] != EMP) break;				\
		if (ban[p -= DIR_ ## dir] != EMP) break;				\
		if (ban[p -= DIR_ ## dir] != EMP) break;				\
		if (ban[p -= DIR_ ## dir] != EMP) break;				\
		p -= DIR_ ## dir;										\
	} while (0)
#define ADDKING2(SG, dir) 

	if (kingS) {	//自玉が盤面にある時のみ有効
#define SetPinS(dir)	\
		p = kingS;		\
		ADDKING1(S, dir);	\
		if (ban[p] != WALL) {	\
			if ((ban[p] & GOTE) == 0) {		\
				if (effectW[p] & (EFFECT_ ## dir << EFFECT_LONG_SHIFT)) pin[p] = DIR_ ## dir;		\
			}		\
			ADDKING2(S, dir);	\
		}

		SetPinS(UP);
		SetPinS(UL);
		SetPinS(UR);
		SetPinS(LEFT);
		SetPinS(RIGHT);
		SetPinS(DL);
		SetPinS(DR);
		SetPinS(DOWN);
#undef SetPinS
	}
	if (kingG) {	//敵玉が盤面にある時のみ有効
#define SetPinG(dir)	\
		p = kingG;		\
		ADDKING1(G, dir);	\
		if (ban[p] != WALL) {		\
			if ((ban[p] & GOTE) != 0) {		\
				if (effectB[p] & (EFFECT_ ## dir << EFFECT_LONG_SHIFT)) pin[p] = DIR_ ## dir;		\
			}	\
			ADDKING2(G, dir);	\
		}

		SetPinG(DOWN);
		SetPinG(DL);
		SetPinG(DR);
		SetPinG(RIGHT);
		SetPinG(LEFT);
		SetPinG(UL);
		SetPinG(UR);
		SetPinG(UP);
#undef SetPinG
	}
#undef ADDKING1
#undef ADDKING2
}

// 利き関係
// effectB/effectWの初期化
void Position::init_effect()
{
	int dan, suji;

	memset(effect, 0, sizeof(effect));

	for (suji = 0x10; suji <= 0x90; suji += 0x10) {
		for (dan = 1 ; dan <= 9 ; dan++) {
			add_effect(suji + dan);
		}
	}
}

void Position::add_effect(const int z)
{
#define ADD_EFFECT(turn,dir) zz = z + DIR_ ## dir; effect[turn][zz] |= EFFECT_ ## dir;

	int zz;

	switch (ban[z]) {

	case EMP:	break;
	case SFU:
		ADD_EFFECT(BLACK, UP);
		break;
	case SKY:
		AddKikiDirS(z, DIR_UP, EFFECT_UP << EFFECT_LONG_SHIFT);
		break;
	case SKE:
		ADD_EFFECT(BLACK, KEUR);
		ADD_EFFECT(BLACK, KEUL);
		break;
	case SGI:
		ADD_EFFECT(BLACK, UP);
		ADD_EFFECT(BLACK, UR);
		ADD_EFFECT(BLACK, UL);
		ADD_EFFECT(BLACK, DR);
		ADD_EFFECT(BLACK, DL);
		break;
	case SKI:
	case STO:
	case SNY:
	case SNK:
	case SNG:
		ADD_EFFECT(BLACK, UP);
		ADD_EFFECT(BLACK, UR);
		ADD_EFFECT(BLACK, UL);
		ADD_EFFECT(BLACK, RIGHT);
		ADD_EFFECT(BLACK, LEFT);
		ADD_EFFECT(BLACK, DOWN);
		break;
	case SUM:
		ADD_EFFECT(BLACK, UP);
		ADD_EFFECT(BLACK, RIGHT);
		ADD_EFFECT(BLACK, LEFT);
		ADD_EFFECT(BLACK, DOWN);
		// 角と同じ利きを追加するため break しない
	case SKA:
		AddKikiDirS(z, DIR_UR, EFFECT_UR << EFFECT_LONG_SHIFT);
		AddKikiDirS(z, DIR_UL, EFFECT_UL << EFFECT_LONG_SHIFT);
		AddKikiDirS(z, DIR_DR, EFFECT_DR << EFFECT_LONG_SHIFT);
		AddKikiDirS(z, DIR_DL, EFFECT_DL << EFFECT_LONG_SHIFT);
		break;
	case SRY:
		ADD_EFFECT(BLACK, UR);
		ADD_EFFECT(BLACK, UL);
		ADD_EFFECT(BLACK, DR);
		ADD_EFFECT(BLACK, DL);
		// 飛と同じ利きを追加するため break しない
	case SHI:
		AddKikiDirS(z, DIR_UP,    EFFECT_UP    << EFFECT_LONG_SHIFT);
		AddKikiDirS(z, DIR_DOWN,  EFFECT_DOWN  << EFFECT_LONG_SHIFT);
		AddKikiDirS(z, DIR_LEFT,  EFFECT_LEFT  << EFFECT_LONG_SHIFT);
		AddKikiDirS(z, DIR_RIGHT, EFFECT_RIGHT << EFFECT_LONG_SHIFT);
		break;
	case SOU:
		ADD_EFFECT(BLACK, UP);
		ADD_EFFECT(BLACK, UR);
		ADD_EFFECT(BLACK, UL);
		ADD_EFFECT(BLACK, RIGHT);
		ADD_EFFECT(BLACK, LEFT);
		ADD_EFFECT(BLACK, DOWN);
		ADD_EFFECT(BLACK, DR);
		ADD_EFFECT(BLACK, DL);
		break;

	case GFU:
		ADD_EFFECT(WHITE, DOWN);
		break;
	case GKY:
		AddKikiDirG(z, DIR_DOWN, EFFECT_DOWN << EFFECT_LONG_SHIFT);
		break;
	case GKE:
		ADD_EFFECT(WHITE, KEDR);
		ADD_EFFECT(WHITE, KEDL);
		break;
	case GGI:
		ADD_EFFECT(WHITE, DOWN);
		ADD_EFFECT(WHITE, DR);
		ADD_EFFECT(WHITE, DL);
		ADD_EFFECT(WHITE, UR);
		ADD_EFFECT(WHITE, UL);
		break;
	case GKI:
	case GTO:
	case GNY:
	case GNK:
	case GNG:
		ADD_EFFECT(WHITE, DOWN);
		ADD_EFFECT(WHITE, DR);
		ADD_EFFECT(WHITE, DL);
		ADD_EFFECT(WHITE, RIGHT);
		ADD_EFFECT(WHITE, LEFT);
		ADD_EFFECT(WHITE, UP);
		break;
	case GUM:
		ADD_EFFECT(WHITE, DOWN);
		ADD_EFFECT(WHITE, RIGHT);
		ADD_EFFECT(WHITE, LEFT);
		ADD_EFFECT(WHITE, UP);
		// 角と同じ利きを追加するため break しない
	case GKA:
		AddKikiDirG(z, DIR_DR, EFFECT_DR << EFFECT_LONG_SHIFT);
		AddKikiDirG(z, DIR_DL, EFFECT_DL << EFFECT_LONG_SHIFT);
		AddKikiDirG(z, DIR_UR, EFFECT_UR << EFFECT_LONG_SHIFT);
		AddKikiDirG(z, DIR_UL, EFFECT_UL << EFFECT_LONG_SHIFT);
		break;
	case GRY:
		ADD_EFFECT(WHITE, DR);
		ADD_EFFECT(WHITE, DL);
		ADD_EFFECT(WHITE, UR);
		ADD_EFFECT(WHITE, UL);
		// 飛と同じ利きを追加するため break しない
	case GHI:
		AddKikiDirG(z, DIR_DOWN,  EFFECT_DOWN  << EFFECT_LONG_SHIFT);
		AddKikiDirG(z, DIR_UP,    EFFECT_UP    << EFFECT_LONG_SHIFT);
		AddKikiDirG(z, DIR_RIGHT, EFFECT_RIGHT << EFFECT_LONG_SHIFT);
		AddKikiDirG(z, DIR_LEFT,  EFFECT_LEFT  << EFFECT_LONG_SHIFT);
		break;
	case GOU:
		ADD_EFFECT(WHITE, DOWN);
		ADD_EFFECT(WHITE, DR);
		ADD_EFFECT(WHITE, DL);
		ADD_EFFECT(WHITE, RIGHT);
		ADD_EFFECT(WHITE, LEFT);
		ADD_EFFECT(WHITE, UP);
		ADD_EFFECT(WHITE, UR);
		ADD_EFFECT(WHITE, UL);
		break;

	case WALL:
	case PIECE_NONE:
	default:__assume(0);
		break;
	}
}

void Position::del_effect(const int z, const Piece kind)
{
#define DEL_EFFECT(turn,dir) zz = z + DIR_ ## dir; effect[turn][zz] &= ~(EFFECT_ ## dir);

	int zz;
	switch (kind) {

	case EMP: break;

	case SFU:
		DEL_EFFECT(BLACK, UP);
		break;
	case SKY:
		DelKikiDirS(z, DIR_UP, ~(EFFECT_UP << EFFECT_LONG_SHIFT));
		break;
	case SKE:
		DEL_EFFECT(BLACK, KEUR);
		DEL_EFFECT(BLACK, KEUL);
		break;
	case SGI:
		DEL_EFFECT(BLACK, UP);
		DEL_EFFECT(BLACK, UR);
		DEL_EFFECT(BLACK, UL);
		DEL_EFFECT(BLACK, DR);
		DEL_EFFECT(BLACK, DL);
		break;
	case SKI:
	case STO:
	case SNY:
	case SNK:
	case SNG:
		DEL_EFFECT(BLACK, UP);
		DEL_EFFECT(BLACK, UR);
		DEL_EFFECT(BLACK, UL);
		DEL_EFFECT(BLACK, RIGHT);
		DEL_EFFECT(BLACK, LEFT);
		DEL_EFFECT(BLACK, DOWN);
		break;
	case SUM:
		DEL_EFFECT(BLACK, UP);
		DEL_EFFECT(BLACK, RIGHT);
		DEL_EFFECT(BLACK, LEFT);
		DEL_EFFECT(BLACK, DOWN);
		// 角と同じ利きを削除するため break しない
	case SKA:
		DelKikiDirS(z, DIR_UR, ~(EFFECT_UR << EFFECT_LONG_SHIFT));
		DelKikiDirS(z, DIR_UL, ~(EFFECT_UL << EFFECT_LONG_SHIFT));
		DelKikiDirS(z, DIR_DR, ~(EFFECT_DR << EFFECT_LONG_SHIFT));
		DelKikiDirS(z, DIR_DL, ~(EFFECT_DL << EFFECT_LONG_SHIFT));
		break;
	case SRY:
		DEL_EFFECT(BLACK, UR);
		DEL_EFFECT(BLACK, UL);
		DEL_EFFECT(BLACK, DR);
		DEL_EFFECT(BLACK, DL);
		// 飛と同じ利きを削除するため break しない
	case SHI:
		DelKikiDirS(z, DIR_UP,    ~(EFFECT_UP    << EFFECT_LONG_SHIFT));
		DelKikiDirS(z, DIR_DOWN,  ~(EFFECT_DOWN  << EFFECT_LONG_SHIFT));
		DelKikiDirS(z, DIR_LEFT,  ~(EFFECT_LEFT  << EFFECT_LONG_SHIFT));
		DelKikiDirS(z, DIR_RIGHT, ~(EFFECT_RIGHT << EFFECT_LONG_SHIFT));
		break;
	case SOU:
		DEL_EFFECT(BLACK, UP);
		DEL_EFFECT(BLACK, UR);
		DEL_EFFECT(BLACK, UL);
		DEL_EFFECT(BLACK, RIGHT);
		DEL_EFFECT(BLACK, LEFT);
		DEL_EFFECT(BLACK, DOWN);
		DEL_EFFECT(BLACK, DR);
		DEL_EFFECT(BLACK, DL);
		break;

	case GFU:
		DEL_EFFECT(WHITE, DOWN);
		break;
	case GKY:
		DelKikiDirG(z, DIR_DOWN, ~(EFFECT_DOWN << EFFECT_LONG_SHIFT));
		break;
	case GKE:
		DEL_EFFECT(WHITE, KEDR);
		DEL_EFFECT(WHITE, KEDL);
		break;
	case GGI:
		DEL_EFFECT(WHITE, DOWN);
		DEL_EFFECT(WHITE, DR);
		DEL_EFFECT(WHITE, DL);
		DEL_EFFECT(WHITE, UR);
		DEL_EFFECT(WHITE, UL);
		break;
	case GKI:
	case GTO:
	case GNY:
	case GNK:
	case GNG:
		DEL_EFFECT(WHITE, DOWN);
		DEL_EFFECT(WHITE, DR);
		DEL_EFFECT(WHITE, DL);
		DEL_EFFECT(WHITE, RIGHT);
		DEL_EFFECT(WHITE, LEFT);
		DEL_EFFECT(WHITE, UP);
		break;
	case GUM:
		DEL_EFFECT(WHITE, UP);
		DEL_EFFECT(WHITE, RIGHT);
		DEL_EFFECT(WHITE, LEFT);
		DEL_EFFECT(WHITE, DOWN);
		// 角と同じ利きを削除するため break しない
	case GKA:
		DelKikiDirG(z, DIR_UR, ~(EFFECT_UR << EFFECT_LONG_SHIFT));
		DelKikiDirG(z, DIR_UL, ~(EFFECT_UL << EFFECT_LONG_SHIFT));
		DelKikiDirG(z, DIR_DR, ~(EFFECT_DR << EFFECT_LONG_SHIFT));
		DelKikiDirG(z, DIR_DL, ~(EFFECT_DL << EFFECT_LONG_SHIFT));
		break;
	case GRY:
		DEL_EFFECT(WHITE, UR);
		DEL_EFFECT(WHITE, UL);
		DEL_EFFECT(WHITE, DR);
		DEL_EFFECT(WHITE, DL);
		// 飛と同じ利きを削除するため break しない
	case GHI:
		DelKikiDirG(z, DIR_UP,    ~(EFFECT_UP    << EFFECT_LONG_SHIFT));
		DelKikiDirG(z, DIR_DOWN,  ~(EFFECT_DOWN  << EFFECT_LONG_SHIFT));
		DelKikiDirG(z, DIR_LEFT,  ~(EFFECT_LEFT  << EFFECT_LONG_SHIFT));
		DelKikiDirG(z, DIR_RIGHT, ~(EFFECT_RIGHT << EFFECT_LONG_SHIFT));
		break;
	case GOU:
		DEL_EFFECT(WHITE, DOWN);
		DEL_EFFECT(WHITE, DR);
		DEL_EFFECT(WHITE, DL);
		DEL_EFFECT(WHITE, RIGHT);
		DEL_EFFECT(WHITE, LEFT);
		DEL_EFFECT(WHITE, UP);
		DEL_EFFECT(WHITE, UR);
		DEL_EFFECT(WHITE, UL);
		break;

	case WALL:
	case PIECE_NONE:
	default:__assume(0);
		break;
	}
}


/// Position::do_move() は手を進める。そして必要なすべての情報を StateInfo オブジェクトに保存する。
/// 手は合法であることを前提としている。Pseudo-legalな手はこの関数を呼ぶ前に取り除く必要がある。

/// Position::do_move() makes a move, and saves all information necessary
/// to a StateInfo object. The move is assumed to be legal. Pseudo-legal
/// moves should be filtered out before this function is called.

void Position::do_move(Move m, StateInfo& newSt)
{
	assert(is_ok());
	assert(&newSt != st);
	assert(!at_checking());
#if defined(MOVE_TRACE)
	m_trace[st->gamePly] = m;
	assert(m != MOVE_NULL);	// NullMoveはdo_null_move()で処理する
#endif

	nodes++;
	Key key = st->key;

	// Copy some fields of old state to our new StateInfo object except the
	// ones which are recalculated from scratch anyway, then switch our state
	// pointer to point to the new, ready to be updated, state.
	/// ※ position.cpp の struct StateInfo と定義を合わせる
	struct ReducedStateInfo {
		int gamePly;
		int pliesFromNull;
		Piece captured;
		uint32_t hand;
		uint32_t effect;
		Key key;
	};

	memcpy(&newSt, st, sizeof(ReducedStateInfo));

	newSt.previous = st;
	st = &newSt;

	// Update side to move
	key ^= zobSideToMove;

	// Increment the 50 moves rule draw counter. Resetting it to zero in the
	// case of non-reversible moves is taken care of later.
	st->pliesFromNull++;

	const Color us = side_to_move();
	if (move_is_drop(m))
	{
		st->key = key;
		do_drop(m);
		st->hand = hand[us].h;
		st->effect = (us == BLACK) ? effectB[kingG] : effectW[kingS];
		assert(!at_checking());
		assert(get_key() == compute_key());
		return;
	}

	const Square from = move_from(m);
	const Square to = move_to(m);
	bool pm = is_promotion(m);

	Piece piece = move_piece(m);
	Piece capture = piece_on(to);
	int kn;
	unsigned long id;
	unsigned long tkiki;

	assert(color_of(piece_on(from)) == us);
	assert(square_is_empty(to) || color_of(piece_on(to)) != us);

	// ピン情報のクリア
	if (piece == SOU) {
		// 先手玉を動かす
		DelPinInfS(DIR_UP);
		DelPinInfS(DIR_DOWN);
		DelPinInfS(DIR_RIGHT);
		DelPinInfS(DIR_LEFT);
		DelPinInfS(DIR_UR);
		DelPinInfS(DIR_UL);
		DelPinInfS(DIR_DR);
		DelPinInfS(DIR_DL);
		if (EFFECT_KING_G(to) /*&& EFFECT_KING_G(to) == ((effectB[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
			_BitScanForward(&id, EFFECT_KING_G(to));
			DelPinInfG(NanohaTbl::Direction[id]);
		}
	} else if (piece == GOU) {
		// 後手玉を動かす
		DelPinInfG(DIR_UP);
		DelPinInfG(DIR_DOWN);
		DelPinInfG(DIR_RIGHT);
		DelPinInfG(DIR_LEFT);
		DelPinInfG(DIR_UR);
		DelPinInfG(DIR_UL);
		DelPinInfG(DIR_DR);
		DelPinInfG(DIR_DL);
		if (EFFECT_KING_S(to) /*&& EFFECT_KING_S(to) == ((effectW[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
			_BitScanForward(&id, EFFECT_KING_S(to));
			DelPinInfS(NanohaTbl::Direction[id]);
		}
	} else {
		if (us == BLACK) {
			// 先手番
			if (EFFECT_KING_S(from)) {
///				_BitScanForward(&id, EFFECT_KING_S(from));
///				DelPinInfS(NanohaTbl::Direction[id]);
				pin[from] = 0;
			}
			if (EFFECT_KING_S(to)/* && (effectW[to] & EFFECT_LONG_MASK)*/) {
				_BitScanForward(&id, EFFECT_KING_S(to));
				DelPinInfS(NanohaTbl::Direction[id]);
			}
///			if (DirTbl[kingG][from] == DirTbl[kingG][to]) {
///				pin[to] = 0;
///			} else 
			{
				if (EFFECT_KING_G(from)) {
					_BitScanForward(&id, EFFECT_KING_G(from));
					DelPinInfG(NanohaTbl::Direction[id]);
				}
				if (EFFECT_KING_G(to)) {
					_BitScanForward(&id, EFFECT_KING_G(to));
					DelPinInfG(NanohaTbl::Direction[id]);
				}
			}
		} else {
			// 後手番
///			if (DirTbl[kingS][from] == DirTbl[kingS][to]) {
///				pin[to] = 0;
///			} else 
			{
				if (EFFECT_KING_S(from)) {
					_BitScanForward(&id, EFFECT_KING_S(from));
					DelPinInfS(NanohaTbl::Direction[id]);
				}
				if (EFFECT_KING_S(to)) {
					_BitScanForward(&id, EFFECT_KING_S(to));
					DelPinInfS(NanohaTbl::Direction[id]);
				}
			}
			if (EFFECT_KING_G(from)) {
//				_BitScanForward(&id, EFFECT_KING_G(from));
//				DelPinInfG(NanohaTbl::Direction[id]);
				pin[from] = 0;
			}
			if (EFFECT_KING_G(to)/* && (effectB[to] & EFFECT_LONG_MASK)*/) {
				_BitScanForward(&id, EFFECT_KING_G(to));
				DelPinInfG(NanohaTbl::Direction[id]);
			}
		}
	}

	del_effect(from, piece);					// 動かす駒の利きを消す
	if (capture) {
		del_effect(to, capture);	// 取る駒の利きを消す
		kn = komano[to];
		knkind[kn] = (capture ^ GOTE) & ~(PROMOTED);
		knpos[kn] = (us == BLACK) ? 1 : 2;
		if (us == BLACK) handS.inc(capture & ~(GOTE | PROMOTED));
		else             handG.inc(capture & ~(GOTE | PROMOTED));

#if !defined(TSUMESOLVER)
		// material 更新
		material -= NanohaTbl::KomaValueEx[capture];
#endif//#if !defined(TSUMESOLVER)

		// ハッシュ更新
		key ^= zobrist[capture][to];
	} else {
		// 移動先は空→移動先の長い利きを消す
		// 後手の利きを消す
		if ((tkiki = effectW[to] & EFFECT_LONG_MASK) != 0) {
			while (tkiki) {
				_BitScanForward(&id, tkiki);
				tkiki &= tkiki-1;
				DelKikiDirG(to, NanohaTbl::Direction[id], ~(1u << id));
			}
		}
		// 先手の利きを消す
		if ((tkiki = effectB[to] & EFFECT_LONG_MASK) != 0) {
			while (tkiki) {
				_BitScanForward(&id, tkiki);
				tkiki &= tkiki-1;
				DelKikiDirS(to, NanohaTbl::Direction[id], ~(1u << id));
			}
		}
	}

	kn = komano[from];
	if (pm) {
#if !defined(TSUMESOLVER)
		// material 更新
		material += NanohaTbl::KomaValuePro[piece];
#endif//#if !defined(TSUMESOLVER)

		piece = Piece(int(piece)|PROMOTED);
	}
	knkind[kn] = piece;
	knpos[kn] = to;

	// ハッシュ更新
	key ^= zobrist[ban[from]][from] ^ zobrist[piece][to];

	// Prefetch TT access as soon as we know key is updated
	prefetch(reinterpret_cast<char*>(TT.first_entry(key)));

	// Move the piece

	ban[to]   = piece;
	ban[from] = EMP;
	komano[to] = kn;
	komano[from] = 0;

	// 利きを更新
	add_effect(to);

	// 移動元の長い利きを伸ばす
	// 後手の利きを書く
	if ((tkiki = effectW[from] & EFFECT_LONG_MASK) != 0) {
		while (tkiki) {
			_BitScanForward(&id, tkiki);
			tkiki &= tkiki-1;
			AddKikiDirG(from, NanohaTbl::Direction[id], 1u << id);
		}
	}
	// 先手の利きを書く
	if ((tkiki = effectB[from] & EFFECT_LONG_MASK) != 0) {
		while (tkiki) {
			_BitScanForward(&id, tkiki);
			tkiki &= tkiki-1;
			AddKikiDirS(from, NanohaTbl::Direction[id], 1u << id);
		}
	}

	// ピン情報の付加
	if (piece == SOU) {
		AddPinInfS(DIR_UP);
		AddPinInfS(DIR_DOWN);
		AddPinInfS(DIR_RIGHT);
		AddPinInfS(DIR_LEFT);
		AddPinInfS(DIR_UR);
		AddPinInfS(DIR_UL);
		AddPinInfS(DIR_DR);
		AddPinInfS(DIR_DL);
		if (EFFECT_KING_G(from) /*&& (effectB[from] & EFFECT_LONG_MASK)*/) {
			_BitScanForward(&id, EFFECT_KING_G(from));
			AddPinInfG(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_G(to) /*&& (effectB[from] & EFFECT_LONG_MASK)*/) {
			_BitScanForward(&id, EFFECT_KING_G(to));
			AddPinInfG(NanohaTbl::Direction[id]);
		}
	} else if (piece == GOU) {
		AddPinInfG(DIR_UP);
		AddPinInfG(DIR_DOWN);
		AddPinInfG(DIR_RIGHT);
		AddPinInfG(DIR_LEFT);
		AddPinInfG(DIR_UR);
		AddPinInfG(DIR_UL);
		AddPinInfG(DIR_DR);
		AddPinInfG(DIR_DL);
		if (EFFECT_KING_S(from) /*&& (effectW[from] & EFFECT_LONG_MASK)*/) {
			_BitScanForward(&id, EFFECT_KING_S(from));
			AddPinInfS(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_S(to) /*&& (effectW[from] & EFFECT_LONG_MASK)*/) {
			_BitScanForward(&id, EFFECT_KING_S(to));
			AddPinInfS(NanohaTbl::Direction[id]);
		}
	} else {
		if (EFFECT_KING_S(from)) {
			_BitScanForward(&id, EFFECT_KING_S(from));
			AddPinInfS(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_S(to)) {
			_BitScanForward(&id, EFFECT_KING_S(to));
			AddPinInfS(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_G(from)) {
			_BitScanForward(&id, EFFECT_KING_G(from));
			AddPinInfG(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_G(to)) {
			_BitScanForward(&id, EFFECT_KING_G(to));
			AddPinInfG(NanohaTbl::Direction[id]);
		}
	}

	// Set capture piece
	st->captured = capture;

	// Update the key with the final value
	st->key = key;
	st->hand = hand[us].h;
	st->effect = (us == BLACK) ? effectB[kingG] : effectW[kingS];

#if !defined(NDEBUG)
	// 手を指したあとに、王手になっている⇒自殺手になっている
	if (in_check()) {
		print_csa(m);
		disp_trace(st->gamePly + 1);
		MYABORT();
	}
#endif

	// Finish
	sideToMove = flip(sideToMove);

#if defined(MOVE_TRACE)
	int fail;
	if (is_ok(&fail) == false) {
		std::cerr << "Error!:is_ok() is false. Reason code = " << fail << std::endl;
		print_csa(m);
	}
#else
	assert(is_ok());
#endif
	assert(get_key() == compute_key());
}

void Position::do_drop(Move m)
{
	const Color us = side_to_move();
	const Square to = move_to(m);

	assert(square_is_empty(to));

	Piece piece = move_piece(m);
	int kn = 0x80;
	int kne = 0;
	unsigned long id;
	unsigned long tkiki;

	// ピン情報のクリア
	if (EFFECT_KING_S(to)/* && EFFECT_KING_S(to) == ((effectW[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
		_BitScanForward(&id, EFFECT_KING_S(to));
		DelPinInfS(NanohaTbl::Direction[id]);
	}
	if (EFFECT_KING_G(to)/* && EFFECT_KING_G(to) == ((effectB[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
		_BitScanForward(&id, EFFECT_KING_G(to));
		DelPinInfG(NanohaTbl::Direction[id]);
	}

	// 移動先は空→移動先の長い利きを消す
	// 後手の利きを消す
	if ((tkiki = effectW[to] & EFFECT_LONG_MASK) != 0) {
		while (tkiki) {
			_BitScanForward(&id, tkiki);
			tkiki &= tkiki-1;
			DelKikiDirG(to, NanohaTbl::Direction[id], ~(1u << id));
		}
	}
	// 先手の利きを消す
	if ((tkiki = effectB[to] & EFFECT_LONG_MASK) != 0) {
		while (tkiki) {
			_BitScanForward(&id, tkiki);
			tkiki &= tkiki-1;
			DelKikiDirS(to, NanohaTbl::Direction[id], ~(1u << id));
		}
	}

	unsigned int diff = 0;
	switch (piece & ~GOTE) {
	case EMP:
		break;
	case FU:
		kn  = KNS_FU;
		kne = KNE_FU;
		diff = HAND_FU_INC;
		break;
	case KY:
		kn  = KNS_KY;
		kne = KNE_KY;
		diff = HAND_KY_INC;
		break;
	case KE:
		kn  = KNS_KE;
		kne = KNE_KE;
		diff = HAND_KE_INC;
		break;
	case GI:
		kn  = KNS_GI;
		kne = KNE_GI;
		diff = HAND_GI_INC;
		break;
	case KI:
		kn  = KNS_KI;
		kne = KNE_KI;
		diff = HAND_KI_INC;
		break;
	case KA:
		kn  = KNS_KA;
		kne = KNE_KA;
		diff = HAND_KA_INC;
		break;
	case HI:
		kn  = KNS_HI;
		kne = KNE_HI;
		diff = HAND_HI_INC;
		break;
	default:
		break;
	}

	if (us == BLACK) {
		handS.h -= diff;
		while (kn <= kne) {
			if (knpos[kn] == 1) break;
			kn++;
		}
	} else {
		handG.h -= diff;
		while (kn <= kne) {
			if (knpos[kn] == 2) break;
			kn++;
		}
	}

#if !defined(NDEBUG)
	// エラーのときに Die!
	if (kn > kne) {
		print_csa(m);
		MYABORT();
	}
#endif

	assert(color_of(piece) == us);

	knkind[kn] = piece;
	knpos[kn] = to;
	ban[to] = piece;
	komano[to] = kn;

	// 利きを更新
	add_effect(to);

	// 移動元、移動先が玉の延長線上にあったときにそこのピン情報を追加する
	if (EFFECT_KING_S(to)) {
		_BitScanForward(&id, EFFECT_KING_S(to));
		AddPinInfS(NanohaTbl::Direction[id]);
	}
	if (EFFECT_KING_G(to)) {
		_BitScanForward(&id, EFFECT_KING_G(to));
		AddPinInfG(NanohaTbl::Direction[id]);
	}

	// Set capture piece
	st->captured = EMP;

	// Update the key with the final value
	st->key ^= zobrist[piece][to];

	// Prefetch TT access as soon as we know key is updated
	prefetch(reinterpret_cast<char*>(TT.first_entry(st->key)));

	// Finish
	sideToMove = flip(sideToMove);

	assert(is_ok());
}

/// Position::undo_move() unmakes a move. When it returns, the position should
/// be restored to exactly the same state as before the move was made.

void Position::undo_move(Move m) {

#if defined(MOVE_TRACE)
	assert(m != MOVE_NULL);	// NullMoveはundo_null_move()で処理する
	int fail;
	if (is_ok(&fail) == false) {
		disp_trace(st->gamePly+1);
		MYABORT();
	}
#else
	assert(is_ok());
#endif
	assert(::is_ok(m));

	sideToMove = flip(sideToMove);

	if (move_is_drop(m))
	{
		undo_drop(m);
		return;
	}

	Color us = side_to_move();
	Square from = move_from(m);
	Square to = move_to(m);
	bool pm = is_promotion(m);
	Piece piece = move_piece(m);
	Piece captured = st->captured;
	int kn;
	unsigned long id;
	unsigned long tkiki;

	assert(square_is_empty(from));
	assert(color_of(piece_on(to)) == us);

	// ピン情報のクリア
	if (piece == SOU) {
		DelPinInfS(DIR_UP);
		DelPinInfS(DIR_DOWN);
		DelPinInfS(DIR_RIGHT);
		DelPinInfS(DIR_LEFT);
		DelPinInfS(DIR_UR);
		DelPinInfS(DIR_UL);
		DelPinInfS(DIR_DR);
		DelPinInfS(DIR_DL);
		if (EFFECT_KING_G(from) /*&& (effectB[from] & EFFECT_LONG_MASK)*/) {
			_BitScanForward(&id, EFFECT_KING_G(from));
			DelPinInfG(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_G(to)) {
			_BitScanForward(&id, EFFECT_KING_G(to));
			DelPinInfG(NanohaTbl::Direction[id]);
		}
	} else if (piece == GOU) {
		DelPinInfG(DIR_UP);
		DelPinInfG(DIR_DOWN);
		DelPinInfG(DIR_RIGHT);
		DelPinInfG(DIR_LEFT);
		DelPinInfG(DIR_UR);
		DelPinInfG(DIR_UL);
		DelPinInfG(DIR_DR);
		DelPinInfG(DIR_DL);
		if (EFFECT_KING_S(from) /*&& (effectW[from] & EFFECT_LONG_MASK)*/) {
			_BitScanForward(&id, EFFECT_KING_S(from));
			DelPinInfS(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_S(to)) {
			_BitScanForward(&id, EFFECT_KING_S(to));
			DelPinInfS(NanohaTbl::Direction[id]);
		}
	} else {
		if (us == BLACK) {
			if (EFFECT_KING_S(from)) {
				_BitScanForward(&id, EFFECT_KING_S(from));
				DelPinInfS(NanohaTbl::Direction[id]);
			}
			if (EFFECT_KING_S(to)) {
				_BitScanForward(&id, EFFECT_KING_S(to));
				DelPinInfS(NanohaTbl::Direction[id]);
///				pin[to] = 0;
			}
			if (EFFECT_KING_G(from)) {
				_BitScanForward(&id, EFFECT_KING_G(from));
				DelPinInfG(NanohaTbl::Direction[id]);
			}
			if (EFFECT_KING_G(to)) {
				_BitScanForward(&id, EFFECT_KING_G(to));
				DelPinInfG(NanohaTbl::Direction[id]);
			}
		} else {
			if (EFFECT_KING_S(from)) {
				_BitScanForward(&id, EFFECT_KING_S(from));
				DelPinInfS(NanohaTbl::Direction[id]);
			}
			if (EFFECT_KING_S(to)) {
				_BitScanForward(&id, EFFECT_KING_S(to));
				DelPinInfS(NanohaTbl::Direction[id]);
			}
			if (EFFECT_KING_G(from)) {
				_BitScanForward(&id, EFFECT_KING_G(from));
				DelPinInfG(NanohaTbl::Direction[id]);
			}
			if (EFFECT_KING_G(to)) {
				_BitScanForward(&id, EFFECT_KING_G(to));
				DelPinInfG(NanohaTbl::Direction[id]);
///				pin[to] = 0;
			}
		}
	}

	del_effect(to, ban[to]);					// 動かした駒の利きを消す

	kn = komano[to];
	if (pm) {
///		piece &= ~PROMOTED;

#if !defined(TSUMESOLVER)
		// material 更新
		material -= NanohaTbl::KomaValuePro[piece];
#endif//#if !defined(TSUMESOLVER)
	}
	knkind[kn] = piece;
	knpos[kn] = from;

	ban[to] = captured;
	komano[from] = komano[to];
	ban[from] = piece;

	if (captured) {
#if !defined(TSUMESOLVER)
		// material 更新
		material += NanohaTbl::KomaValueEx[captured];
#endif//#if !defined(TSUMESOLVER)

		int kne = 0;
		switch (captured & ~(GOTE|PROMOTED)) {
		case EMP:
			break;
		case FU:
			kn  = KNS_FU;
			kne = KNE_FU;
			break;
		case KY:
			kn  = KNS_KY;
			kne = KNE_KY;
			break;
		case KE:
			kn  = KNS_KE;
			kne = KNE_KE;
			break;
		case GI:
			kn  = KNS_GI;
			kne = KNE_GI;
			break;
		case KI:
			kn  = KNS_KI;
			kne = KNE_KI;
			break;
		case KA:
			kn  = KNS_KA;
			kne = KNE_KA;
			break;
		case HI:
			kn  = KNS_HI;
			kne = KNE_HI;
			break;
		default:
			break;
		}
	
		while (kn <= kne) {
			if (us == BLACK) {
				if (knpos[kn] == 1) break;
			} else {
				if (knpos[kn] == 2) break;
			}
			kn++;
		}
#if 0
		// エラーのときに Die!
		if (kn > kne) {
			Print();
			move_print(m);
			output_info(":kn=%d, kne=%d, capture=0x%X\n", kn, kne, captured);
			MYABORT();
		}
#endif
		knkind[kn] = captured;
		knpos[kn] = to;
		ban[to] = captured;
		komano[to] = kn;
		add_effect(to);	// 取った駒の利きを追加

		if (us == BLACK) handS.dec(captured & ~(GOTE | PROMOTED));
		else             handG.dec(captured & ~(GOTE | PROMOTED));
	} else {
		// 移動先は空→移動先の長い利きを通す
		// 後手の利きを消す
		if ((tkiki = effectW[to] & EFFECT_LONG_MASK) != 0) {
			while (tkiki) {
				_BitScanForward(&id, tkiki);
				tkiki &= tkiki-1;
				AddKikiDirG(to, NanohaTbl::Direction[id], 1u << id);
			}
		}
		// 先手の利きを消す
		if ((tkiki = effectB[to] & EFFECT_LONG_MASK) != 0) {
			while (tkiki) {
				_BitScanForward(&id, tkiki);
				tkiki &= tkiki-1;
				AddKikiDirS(to, NanohaTbl::Direction[id], 1u << id);
			}
		}
		ban[to] = EMP;
		komano[to] = 0;
	}

	// 移動元の長い利きをさえぎる
	// 後手の利きを消す
	if ((tkiki = effectW[from] & EFFECT_LONG_MASK) != 0) {
		while (tkiki) {
			_BitScanForward(&id, tkiki);
			tkiki &= tkiki-1;
			DelKikiDirG(from, NanohaTbl::Direction[id], ~(1u << id));
			if (piece == SOU) {
				// 長い利きは玉を一つだけ貫く
				if (ban[from + NanohaTbl::Direction[id]] != WALL) effectW[from + NanohaTbl::Direction[id]] |= (1u << id);
			}
		}
	}
	// 先手の利きを消す
	if ((tkiki = effectB[from] & EFFECT_LONG_MASK) != 0) {
		while (tkiki) {
			_BitScanForward(&id, tkiki);
			tkiki &= tkiki-1;
			DelKikiDirS(from, NanohaTbl::Direction[id], ~(1u << id));
			if (piece == GOU) {
				// 長い利きは玉を一つだけ貫く
				if (ban[from + NanohaTbl::Direction[id]] != WALL) effectB[from + NanohaTbl::Direction[id]] |= (1u << id);
			}
		}
	}

	// 利きを更新
	add_effect(from);

	// ピン情報付加
	if (piece == SOU) {
		AddPinInfS(DIR_UP);
		AddPinInfS(DIR_DOWN);
		AddPinInfS(DIR_RIGHT);
		AddPinInfS(DIR_LEFT);
		AddPinInfS(DIR_UR);
		AddPinInfS(DIR_UL);
		AddPinInfS(DIR_DR);
		AddPinInfS(DIR_DL);
		if (EFFECT_KING_G(from)) {
			_BitScanForward(&id, EFFECT_KING_G(from));
			AddPinInfG(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_G(to) /*&& (effectB[to] & EFFECT_LONG_MASK)*/) {
			_BitScanForward(&id, EFFECT_KING_G(to));
			AddPinInfG(NanohaTbl::Direction[id]);
		}
	} else if (piece == GOU) {
		AddPinInfG(DIR_UP);
		AddPinInfG(DIR_DOWN);
		AddPinInfG(DIR_RIGHT);
		AddPinInfG(DIR_LEFT);
		AddPinInfG(DIR_UR);
		AddPinInfG(DIR_UL);
		AddPinInfG(DIR_DR);
		AddPinInfG(DIR_DL);
		if (EFFECT_KING_S(from)) {
			_BitScanForward(&id, EFFECT_KING_S(from));
			AddPinInfS(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_S(to) /*&& (effectW[to] & EFFECT_LONG_MASK)*/) {
			_BitScanForward(&id, EFFECT_KING_S(to));
			AddPinInfS(NanohaTbl::Direction[id]);
		}
	} else {
		if (EFFECT_KING_S(from)) {
			_BitScanForward(&id, EFFECT_KING_S(from));
			AddPinInfS(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_S(to)) {
			_BitScanForward(&id, EFFECT_KING_S(to));
			AddPinInfS(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_G(from)) {
			_BitScanForward(&id, EFFECT_KING_G(from));
			AddPinInfG(NanohaTbl::Direction[id]);
		}
		if (EFFECT_KING_G(to)) {
			_BitScanForward(&id, EFFECT_KING_G(to));
			AddPinInfG(NanohaTbl::Direction[id]);
		}
	}

	// Finally point our state pointer back to the previous state
	st = st->previous;

	assert(is_ok());
}

void Position::undo_drop(Move m)
{
	Color us = side_to_move();
	Square to = move_to(m);
	Piece piece = move_piece(m);
	int kn = 0x80;
	int kne = 0;
	unsigned long id;
	unsigned long tkiki;

	assert(color_of(piece_on(to)) == us);

	// 移動元、移動先が玉の延長線上にあったときにそこのピン情報を削除する
	if (EFFECT_KING_S(to)) {
		_BitScanForward(&id, EFFECT_KING_S(to));
		DelPinInfS(NanohaTbl::Direction[id]);
	}
	if (EFFECT_KING_G(to)) {
		_BitScanForward(&id, EFFECT_KING_G(to));
		DelPinInfG(NanohaTbl::Direction[id]);
	}

	unsigned int diff = 0;
	switch (piece & ~GOTE) {
	case EMP:
		break;
	case FU:
		kn  = KNS_FU;
		kne = KNE_FU;
		diff = HAND_FU_INC;
		break;
	case KY:
		kn  = KNS_KY;
		kne = KNE_KY;
		diff = HAND_KY_INC;
		break;
	case KE:
		kn  = KNS_KE;
		kne = KNE_KE;
		diff = HAND_KE_INC;
		break;
	case GI:
		kn  = KNS_GI;
		kne = KNE_GI;
		diff = HAND_GI_INC;
		break;
	case KI:
		kn  = KNS_KI;
		kne = KNE_KI;
		diff = HAND_KI_INC;
		break;
	case KA:
		kn  = KNS_KA;
		kne = KNE_KA;
		diff = HAND_KA_INC;
		break;
	case HI:
		kn  = KNS_HI;
		kne = KNE_HI;
		diff = HAND_HI_INC;
		break;
	default:
		break;
	}

	while (kn <= kne) {
		if (knpos[kn] == to) break;
		kn++;
	}
#if 0
	// エラーのときに Die!
	if (kn > kne) {
		Print();
		move_print(m);
		output_info("\n");
		MYABORT();
	}
#endif

	knkind[kn] = piece;
	knpos[kn] = (us == BLACK) ? 1 : 2;
	ban[to] = EMP;
	komano[to] = 0;

	del_effect(to, piece);					// 動かした駒の利きを消す

	// 打った位置の長い利きを通す
	// 後手の利きを追加
	if ((tkiki = effectW[to] & EFFECT_LONG_MASK) != 0) {
		while (tkiki) {
			_BitScanForward(&id, tkiki);
			tkiki &= tkiki-1;
			AddKikiDirG(to, NanohaTbl::Direction[id], 1u << id);
		}
	}
	// 先手の利きを追加
	if ((tkiki = effectB[to] & EFFECT_LONG_MASK) != 0) {
		while (tkiki) {
			_BitScanForward(&id, tkiki);
			tkiki &= tkiki-1;
			AddKikiDirS(to, NanohaTbl::Direction[id], 1u << id);
		}
	}

	// 移動元、移動先が玉の延長線上にあったときにそこのピン情報を追加する
	if (EFFECT_KING_S(to) /*&& EFFECT_KING_S(to) == ((effectW[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
		_BitScanForward(&id, EFFECT_KING_S(to));
		AddPinInfS(NanohaTbl::Direction[id]);
	}
	if (EFFECT_KING_G(to) /*&& EFFECT_KING_G(to) == ((effectB[to] & EFFECT_LONG_MASK) >> EFFECT_LONG_SHIFT)*/) {
		_BitScanForward(&id, EFFECT_KING_G(to));
		AddPinInfG(NanohaTbl::Direction[id]);
	}

	if (us == BLACK) handS.h += diff;
	else             handG.h += diff;

	// Finally point our state pointer back to the previous state
	st = st->previous;

	assert(is_ok());
}

// 手を進めずにハッシュ計算のみ行う
uint64_t Position::calc_hash_no_move(const Move m) const
{
	uint64_t new_key = get_key();

	new_key ^= zobSideToMove;	// 手番反転

	// from の計算
	int from = move_from(m);
	int to = move_to(m);
	int piece = move_piece(m);
	if (!move_is_drop(m)) {
		// 空白になったことで変わるハッシュ値
		new_key ^= zobrist[piece][from];
	}

	// to の処理
	// ban[to]にあったものをＨａｓｈから消す
	Piece capture = move_captured(m);
	if (capture) {
		new_key ^= zobrist[ban[to]][to];
	}

	// 新しい駒をＨａｓｈに加える
	if (is_promotion(m)) piece |= PROMOTED;
	new_key ^= zobrist[piece][to];

	return new_key;
}

// 指し手チェック系
// 指し手が王手かどうかチェックする
bool Position::is_check_move(const Color us, Move m) const
{
	const Square ksq = (us == BLACK) ? Square(kingG) : Square(kingS);	// 相手側の玉の位置

	return move_attacks_square(m, ksq);
}

// 指し手mによって、升目sqに利きがつくか？
bool Position::move_attacks_square(Move m, Square sq) const
{
	const Color us = side_to_move();
	const effect_t *akiki = (us == BLACK) ? effectB : effectW;	// 自分の利き
	const Piece piece = is_promotion(m) ? Piece(move_piece(m) | PROMOTED) : move_piece(m);
	const Square to = move_to(m);

	switch (piece) {
	case EMP:break;
	case SFU:
		if (to + DIR_UP == sq) return true;
		break;
	case SKY:
		if (DirTbl[to][sq] == EFFECT_UP) {
			if (SkipOverEMP(to, DIR_UP) == sq) return true;
		}
		break;
	case SKE:
		if (to + DIR_KEUR == sq) return true;
		if (to + DIR_KEUL == sq) return true;
		break;
	case SGI:
		if (to + DIR_UP == sq) return true;
		if (to + DIR_UR == sq) return true;
		if (to + DIR_UL == sq) return true;
		if (to + DIR_DR == sq) return true;
		if (to + DIR_DL == sq) return true;
		break;
	case SKI:
	case STO:
	case SNY:
	case SNK:
	case SNG:
		if (to + DIR_UP    == sq) return true;
		if (to + DIR_UR    == sq) return true;
		if (to + DIR_UL    == sq) return true;
		if (to + DIR_RIGHT == sq) return true;
		if (to + DIR_LEFT  == sq) return true;
		if (to + DIR_DOWN  == sq) return true;
		break;

	case GFU:
		if (to + DIR_DOWN == sq) return true;
		break;
	case GKY:
		if (DirTbl[to][sq] == EFFECT_DOWN) {
			if (SkipOverEMP(to, DIR_DOWN) == sq) return true;
		}
		break;
	case GKE:
		if (to + DIR_KEDR == sq) return true;
		if (to + DIR_KEDL == sq) return true;
		break;
	case GGI:
		if (to + DIR_DOWN == sq) return true;
		if (to + DIR_DR   == sq) return true;
		if (to + DIR_DL   == sq) return true;
		if (to + DIR_UR   == sq) return true;
		if (to + DIR_UL   == sq) return true;
		break;
	case GKI:
	case GTO:
	case GNY:
	case GNK:
	case GNG:
		if (to + DIR_DOWN  == sq) return true;
		if (to + DIR_DR    == sq) return true;
		if (to + DIR_DL    == sq) return true;
		if (to + DIR_RIGHT == sq) return true;
		if (to + DIR_LEFT  == sq) return true;
		if (to + DIR_UP    == sq) return true;
		break;

	case SUM:
	case GUM:
		if (to + DIR_UP    == sq) return true;
		if (to + DIR_RIGHT == sq) return true;
		if (to + DIR_LEFT  == sq) return true;
		if (to + DIR_DOWN  == sq) return true;
		// Through
	case SKA:
	case GKA:
		if ((DirTbl[to][sq] & (EFFECT_UR | EFFECT_UL | EFFECT_DR | EFFECT_DL)) != 0) {
			if ((DirTbl[to][sq] & EFFECT_UR) != 0) {
				if (SkipOverEMP(to, DIR_UR) == sq) return true;
			}
			if ((DirTbl[to][sq] & EFFECT_UL) != 0) {
				if (SkipOverEMP(to, DIR_UL) == sq) return true;
			}
			if ((DirTbl[to][sq] & EFFECT_DR) != 0) {
				if (SkipOverEMP(to, DIR_DR) == sq) return true;
			}
			if ((DirTbl[to][sq] & EFFECT_DL) != 0) {
				if (SkipOverEMP(to, DIR_DL) == sq) return true;
			}
		}
		break;
	case SRY:
	case GRY:
		if (to + DIR_UR == sq) return true;
		if (to + DIR_UL == sq) return true;
		if (to + DIR_DR == sq) return true;
		if (to + DIR_DL == sq) return true;
		// Through
	case SHI:
	case GHI:
		if ((DirTbl[to][sq] & (EFFECT_UP | EFFECT_RIGHT | EFFECT_LEFT | EFFECT_DOWN)) != 0) {
			if ((DirTbl[to][sq] & EFFECT_UP) != 0) {
				if (SkipOverEMP(to, DIR_UP) == sq) return true;
			}
			if ((DirTbl[to][sq] & EFFECT_DOWN) != 0) {
				if (SkipOverEMP(to, DIR_DOWN) == sq) return true;
			}
			if ((DirTbl[to][sq] & EFFECT_RIGHT) != 0) {
				if (SkipOverEMP(to, DIR_RIGHT) == sq) return true;
			}
			if ((DirTbl[to][sq] & EFFECT_LEFT) != 0) {
				if (SkipOverEMP(to, DIR_LEFT) == sq) return true;
			}
		}
		break;
	case SOU:
	case GOU:
	case WALL:
	case PIECE_NONE:
	default:
		break;
	}

	// 駒が移動することによってsqに利くか.
	const int from = move_from(m);
	if (from < 0x11) return false;
	if ((DirTbl[from][sq]) & (akiki[from] >> EFFECT_LONG_SHIFT)) {
		if (DirTbl[from][sq] == DirTbl[to][sq]) return false;

		unsigned long id;
		_BitScanForward(&id, DirTbl[from][sq]);
		if (SkipOverEMP(from, NanohaTbl::Direction[id]) == sq) {
			return true;
		}
	}

	return false;
}
bool Position::move_gives_check(Move m) const
{
	const Color us = side_to_move();
	return is_check_move(us, m);
}


// 合法手か確認する
bool Position::pl_move_is_legal(const Move m) const
{
	const Piece piece = move_piece(m);
	const Color us = side_to_move();

	// 自分の駒でない駒を動かしているか？
	if (us != color_of(piece)) return false;

	const PieceType pt = type_of(piece);
	const int to = move_to(m);
	const int from = move_from(m);
	if (from == to) return false;

	if (move_is_drop(m)) {
		// 打つ駒を持っているか？
		const Hand &h = (us == BLACK) ? handS : handG;
		if ( !h.exist(piece)) {
			return false;
		}
		if (ban[to] != EMP) {
			return false;
		}
		if (pt == FU) {
			// 二歩と打ち歩詰めのチェック
			if (is_double_pawn(us, to)) return false;
			if (is_pawn_drop_mate(us, to)) return false;
			// 行き所のない駒打ちは生成しないはずなので、チェックを省略可か？
			if (us == BLACK) return is_drop_pawn<BLACK>(to);
			if (us == WHITE) return is_drop_pawn<WHITE>(to);
		} else if (pt == KY) {
			// 行き所のない駒打ちは生成しないはずなので、チェックを省略可か？
			if (us == BLACK) return is_drop_pawn<BLACK>(to);
			if (us == WHITE) return is_drop_pawn<WHITE>(to);
		} else if (pt == KE) {
			// 行き所のない駒打ちは生成しないはずなので、チェックを省略可か？
			if (us == BLACK) return is_drop_knight<BLACK>(to);
			if (us == WHITE) return is_drop_knight<WHITE>(to);
		}
	} else {
		// 動かす駒が存在するか？
#if !defined(NDEBUG)
		if (DEBUG_LEVEL > 0) {
			std::cerr << "Color=" << int(us) << ", sideToMove=" << int(sideToMove) << std::endl;
			std::cerr << "Move : from=0x" << std::hex << from << ", to=0x" << to << std::endl;
			std::cerr << "   piece=" << int(piece) << ", cap=" << int(move_captured(m)) << std::endl;
			std::cerr << "   ban[from]=" << int(ban[from]) << ", ban[to]=" << int(ban[to]) << std::endl;
		}
#endif
		if (ban[from] != piece) {
			return false;
		}
		if (ban[to] == WALL) {
			return false;
		}
		if (ban[to] != EMP && color_of(ban[to]) == us) {
			// 自分の駒を取っている
			return false;
		}
		// 玉の場合、自殺はできない
		if (move_ptype(m) == OU) {
			Color them = flip(sideToMove);
			if (effect[them][to]) return false;
		}
		// ピンの場合、ピンの方向にしか動けない。
		if (pin[from]) {
			int kPos = (us == BLACK) ? kingS : kingG;
			if (DirTbl[kPos][to] != DirTbl[kPos][from]) return false;
		}
		// TODO:駒を飛び越えないか？
		int d = Max(abs((from >> 4)-(to >> 4)), abs((from & 0x0F)-(to&0x0F)));
		if (pt == KE) {
			if (d != 2) return false;
		} else if (d > 1) {
			// 香、角、飛、馬、龍しかない.
			// 移動の途中をチェックする
			int dir = (to - from) / d;
			if (((to - from) % d) != 0) return false;
			for (int i = 1, z = from + dir; i < d; i++, z += dir) {
				if (ban[z] != EMP) return false;
			}
		}
	}

#if 0
	// TODO:行き所のない駒、二歩、打ち歩詰め、駒を飛び越えないか？等のチェック
	// captureを盤上の駒に設定
	if (IsCorrectMove(m)) {
		if (piece == SOU || piece == GOU) return 1;

		// 自玉に王手をかけていないか、実際に動かして調べる
		Position kk(*this);
		StateInfo newSt;
		kk.do_move(m, newSt);
		if (us == BLACK && kingS && EXIST_EFFECT(kk.effectW[kingS])) {
			return false;
		}
		if (us != BLACK &&  kingG && EXIST_EFFECT(kk.effectB[kingG])) {
			return false;
		}
		return true;
	}
	return false;
#else
	return true;
#endif
}

// 指定場所(to)が打ち歩詰めになるか確認する
bool Position::is_pawn_drop_mate(const Color us, int to) const
{
	// まず、玉の頭に歩を打つ手じゃなければ打ち歩詰めの心配はない。
	if (us == BLACK) {
		if (kingG + DIR_DOWN != to) {
			return 0;
		}
	} else {
		if (kingS + DIR_UP != to) {
			return 0;
		}
	}

	Piece piece;

	// 歩を取れるか？
	if (us == BLACK) {
		// 自分の利きがないなら玉で取れる
		if (! EXIST_EFFECT(effectB[to])) return 0;

		// 取る動きを列挙してみたら玉で取る手しかない
		if ((EXIST_EFFECT(effectW[to]) & ~EFFECT_DOWN) != 0) {
			// 玉以外で取れる手がある【課題】pinの考慮
			effect_t kiki = effectW[to] & (EFFECT_SHORT_MASK & ~EFFECT_DOWN);
			unsigned long id;
			while (kiki) {
				_BitScanForward(&id, kiki);
				kiki &= (kiki - 1);
				if (pin[to - NanohaTbl::Direction[id]] == 0) return 0;
			}
			kiki = effectW[to] & EFFECT_LONG_MASK;
			while (kiki) {
				_BitScanForward(&id, kiki);
				kiki &= (kiki - 1);
				if (pin[SkipOverEMP(to, -NanohaTbl::Direction[id])] == 0) return 0;
			}
		}
		// 玉に逃げ道があるかどうかをチェック
		if (effectB[to] & ((EFFECT_LEFT|EFFECT_RIGHT|EFFECT_UR|EFFECT_UL) << EFFECT_LONG_SHIFT)) {
			if ((effectB[to] & (EFFECT_LEFT  << EFFECT_LONG_SHIFT))
			  && (ban[to+DIR_LEFT ] != WALL && (ban[to+DIR_LEFT ] & GOTE) == 0)
			  && ((effectB[to+DIR_LEFT ] & ~(EFFECT_LEFT  << EFFECT_LONG_SHIFT)) == 0)) {
				return 0;
			}
			if ((effectB[to] & (EFFECT_RIGHT << EFFECT_LONG_SHIFT))
			  && (ban[to+DIR_RIGHT] != WALL && (ban[to+DIR_RIGHT] & GOTE) == 0)
			  && ((effectB[to+DIR_RIGHT] & ~(EFFECT_RIGHT << EFFECT_LONG_SHIFT)) == 0)) {
				return 0;
			}
			if ((effectB[to] & (EFFECT_UR    << EFFECT_LONG_SHIFT))
			  && (ban[to+DIR_UR   ] != WALL && (ban[to+DIR_UR   ] & GOTE) == 0)
			  && ((effectB[to+DIR_UR   ] & ~(EFFECT_UR    << EFFECT_LONG_SHIFT)) == 0)) {
				return 0;
			}
			if ((effectB[to] & (EFFECT_UL    << EFFECT_LONG_SHIFT))
			  && (ban[to+DIR_UL   ] != WALL && (ban[to+DIR_UL   ] & GOTE) == 0)
			  && ((effectB[to+DIR_UL   ] & ~(EFFECT_UL    << EFFECT_LONG_SHIFT)) == 0)) {
				return 0;
			}
		}
#define EscapeG(dir)	piece = ban[kingG + DIR_##dir];	\
						if (piece != WALL && !(piece & GOTE) && !EXIST_EFFECT(effectB[kingG + DIR_##dir])) return 0
		EscapeG(UP);
		EscapeG(UR);
		EscapeG(UL);
		EscapeG(RIGHT);
		EscapeG(LEFT);
		EscapeG(DR);
		EscapeG(DL);
#undef EscapeG

		// 玉の逃げ道もないのなら、打ち歩詰め。
		return 1;
	} else {
		// 自分の利きがないなら玉で取れる
		if (! EXIST_EFFECT(effectW[to])) return 0;

		// 取る動きを列挙してみたら玉で取る手しかない
		if ((EXIST_EFFECT(effectB[to]) & ~EFFECT_UP) != 0) {
			// 玉以外で取れる手がある【課題】pinの考慮
			effect_t kiki = effectB[to] & (EFFECT_SHORT_MASK & ~EFFECT_UP);
			unsigned long id;
			while (kiki) {
				_BitScanForward(&id, kiki);
				kiki &= (kiki - 1);
				if (pin[to - NanohaTbl::Direction[id]] == 0) return 0;
			}
			kiki = effectB[to] & EFFECT_LONG_MASK;
			while (kiki) {
				_BitScanForward(&id, kiki);
				kiki &= (kiki - 1);
				if (pin[SkipOverEMP(to, -NanohaTbl::Direction[id])] == 0) return 0;
			}
		}
		// 玉に逃げ道があるかどうかをチェック
		if (effectW[to] & ((EFFECT_LEFT|EFFECT_RIGHT|EFFECT_DR|EFFECT_DL) << EFFECT_LONG_SHIFT)) {
			if ((effectW[to] & (EFFECT_LEFT  << EFFECT_LONG_SHIFT))
			  && (ban[to+DIR_LEFT ] == EMP || (ban[to+DIR_LEFT ] & GOTE))
			  && ((effectW[to+DIR_LEFT ] & ~(EFFECT_LEFT  << EFFECT_LONG_SHIFT)) == 0)) {
				return 0;
			}
			if ((effectW[to] & (EFFECT_RIGHT << EFFECT_LONG_SHIFT))
			  && (ban[to+DIR_RIGHT] == EMP || (ban[to+DIR_RIGHT] & GOTE))
			  && ((effectW[to+DIR_RIGHT] & ~(EFFECT_RIGHT << EFFECT_LONG_SHIFT)) == 0)) {
				return 0;
			}
			if ((effectW[to] & (EFFECT_DR    << EFFECT_LONG_SHIFT))
			  && (ban[to+DIR_DR   ] == EMP || (ban[to+DIR_DR   ] & GOTE))
			  && ((effectW[to+DIR_DR   ] & ~(EFFECT_DR    << EFFECT_LONG_SHIFT)) == 0)) {
				return 0;
			}
			if ((effectW[to] & (EFFECT_DL    << EFFECT_LONG_SHIFT))
			  && (ban[to+DIR_DL   ] == EMP || (ban[to+DIR_DL   ] & GOTE))
			  && ((effectW[to+DIR_DL   ] & ~(EFFECT_DL    << EFFECT_LONG_SHIFT)) == 0)) {
				return 0;
			}
		}
#define EscapeS(dir)	piece = ban[kingS + DIR_##dir];	\
						if ((piece == EMP || (piece & GOTE)) && !EXIST_EFFECT(effectW[kingS + DIR_##dir])) return 0
		EscapeS(DOWN);
		EscapeS(DR);
		EscapeS(DL);
		EscapeS(RIGHT);
		EscapeS(LEFT);
		EscapeS(UR);
		EscapeS(UL);
#undef EscapeG

		// 玉の逃げ道もないのなら、打ち歩詰め。
		return 1;
	}
}

// Move Generator系

// 手生成
template<Color us>
MoveStack* Position::add_straight(MoveStack* mlist, const int from, const int dir) const
{
	int z_pin = this->pin[from];
	if (z_pin == 0 || abs(z_pin) == abs(dir)) {
		// 空白の間、動く手を生成する
		int to;
		int dan;
		int fromDan = from & 0x0f;
		bool promote = can_promotion<us>(fromDan);
		const Piece piece = ban[from];
		unsigned int tmp = From2Move(from) | Piece2Move(piece);
		for (to = from + dir; ban[to] == EMP; to += dir) {
			dan = to & 0x0f;
			promote |= can_promotion<us>(dan);
			tmp &= ~TO_MASK;
			tmp |= To2Move(to);
			if (promote && (piece & PROMOTED) == 0) {
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				if (us == BLACK && piece == SKY) {
					if (dan > 1) {
						(mlist++)->move = Move(tmp);
					}
				} else if (us == WHITE && piece == GKY) {
					if (dan < 9) {
						(mlist++)->move = Move(tmp);
					}
				} else {
					// 角・飛車
					// 成らない手も生成する。
					(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
				}
			} else {
				// 成れないときと馬・龍
				(mlist++)->move = Move(tmp);
			}
		}
		// 味方の駒でないなら、そこへ動く
		if ((us == BLACK && (ban[to] != WALL) && (ban[to] & GOTE))
		 || (us == WHITE && (ban[to] != WALL) && (ban[to] & GOTE) == 0)) {
			dan = to & 0x0f;
			promote |= can_promotion<us>(dan);
			tmp &= ~TO_MASK;
			tmp |= To2Move(to) | Cap2Move(ban[to]);
			if (promote && (piece & PROMOTED) == 0) {
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				if (piece == SKY) {
					if (dan > 1) {
						(mlist++)->move = Move(tmp);
					}
				} else if (piece == GKY) {
					if (dan < 9) {
						(mlist++)->move = Move(tmp);
					}
				} else {
					// 角・飛車
					// 成らない手も生成する。
					(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
				}
			} else {
				// 成れないときと馬・龍
				(mlist++)->move = Move(tmp);
			}
		}
	}
	return mlist;
}

template<Color us>
MoveStack* Position::add_move(MoveStack* mlist, const int from, const int dir) const
{
	const int to = from + dir;
	const Piece capture = ban[to];
	if ((capture == EMP) 
		 || (us == BLACK &&  (capture & GOTE))
		 || (us == WHITE && ((capture & GOTE) == 0 && capture != WALL))
	) {
		const int piece = ban[from];
		int dan = to & 0x0f;
		int fromDan = from & 0x0f;
		bool promote = can_promotion<us>(dan) || can_promotion<us>(fromDan);
		unsigned int tmp = From2Move(from) | To2Move(to) | Piece2Move(piece) | Cap2Move(capture);
		if (promote) {
			const int kind = piece & ~GOTE;
			switch (kind) {
			case SFU:
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				if (is_drop_pawn<us>(dan)) {
					// 成らない手も生成する。
					(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
				}
				break;
			case SKY:
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				if (is_drop_pawn<us>(dan)) {
					// 成らない手も生成する。
					(mlist++)->move = Move(tmp);
				}
				break;
			case SKE:
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				if (is_drop_knight<us>(dan)) {
					// 成らない手も生成する。
					(mlist++)->move = Move(tmp);
				}
				break;
			case SGI:
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				(mlist++)->move = Move(tmp);
				break;
			case SKA:
			case SHI:
				(mlist++)->move = Move(tmp | FLAG_PROMO);
				// 成らない手も生成する。
				(mlist++)->move = Move(tmp | MOVE_CHECK_NARAZU);
				break;
			default:
				(mlist++)->move = Move(tmp);
				break;
			}
		} else {
			// 成れない
			(mlist++)->move = Move(tmp);
		}
	}
	return mlist;
}

// 指定場所(to)に動く手の生成（玉以外）
MoveStack* Position::gen_move_to(const Color us, MoveStack* mlist, int to) const
{
	effect_t efft = (us == BLACK) ? this->effectB[to] : this->effectW[to];

	// 指定場所に利いている駒がない
	if ((efft & (EFFECT_SHORT_MASK | EFFECT_LONG_MASK)) == 0) return mlist;

	int z;
	int pn;

	// 飛びの利き
	effect_t long_effect = efft & EFFECT_LONG_MASK;
	while (long_effect) {
		unsigned long id;
		_BitScanForward(&id, long_effect);
		id -= EFFECT_LONG_SHIFT;
		long_effect &= long_effect - 1;

		z = SkipOverEMP(to, -NanohaTbl::Direction[id]);
		pn = pin[z];
		if (pn == 0 || abs(pn) == abs(NanohaTbl::Direction[id])) {
			mlist = (us == BLACK) ? add_moveB(mlist, z, to - z) : add_moveW(mlist, z, to - z);
		}
	}

	// 短い利き
	efft &= EFFECT_SHORT_MASK;
	while (efft) {
		unsigned long id;
		_BitScanForward(&id, efft);
		efft &= efft - 1;

		z = to - NanohaTbl::Direction[id];
		pn = pin[z];
		if (pn == 0 || abs(pn) == abs(NanohaTbl::Direction[id])) {
			if (us == BLACK) {
				if (ban[z] != SOU) mlist = add_moveB(mlist, z, to - z);
			} else {
				if (ban[z] != GOU) mlist = add_moveW(mlist, z, to - z);
			}
		}
	}
	return mlist;
}

// 指定場所(to)に駒を打つ手の生成
MoveStack* Position::gen_drop_to(const Color us, MoveStack* mlist, int to) const
{
	int dan = to & 0x0f;
	if (us != BLACK) {
		dan = 10 - dan;
	}
	const Hand &h = (us == BLACK) ? handS : handG;
	const int SorG = (us == BLACK) ? SENTE : GOTE;
#define SetTe(koma)	\
	if (h.get ## koma() > 0) {		\
		(mlist++)->move = Move(To2Move(to) | Piece2Move(SorG|koma));		\
	}
	
	if (h.getFU() > 0 && dan > 1) {
		// 歩を打つ手を生成
		// 二歩チェック
		int nifu = is_double_pawn(us, to & 0xF0);
		// 打ち歩詰めもチェック
		if (!nifu && !is_pawn_drop_mate(us, to)) {
			(mlist++)->move = Move(To2Move(to) | Piece2Move(SorG | FU));
		}
	}
	if (h.getKY() > 0 && dan > 1) {
		// 香を打つ手を生成
		(mlist++)->move = Move(To2Move(to) | Piece2Move(SorG|KY));
	}
	if (h.getKE() > 0 && dan > 2) {
		(mlist++)->move = Move(To2Move(to) | Piece2Move(SorG|KE));
	}
	SetTe(GI)
	SetTe(KI)
	SetTe(KA)
	SetTe(HI)
#undef SetTe
	return mlist;
}

// 駒を打つ手の生成
template <Color us>
MoveStack* Position::gen_drop(MoveStack* mlist) const
{
	int z;
	int suji;
	unsigned int tmp;
	int StartDan;

#if defined(DEBUG_GENERATE)
	MoveStack* top = mlist;
#endif
///	int teNum = teNumM;	// アドレスを取らない
	// 歩を打つ
	uint32_t exists;
	exists = (us == BLACK) ? handS.existFU() : handG.existFU();
	if (exists > 0) {
		tmp  = (us == BLACK) ? Piece2Move(SFU) : Piece2Move(GFU);	// From = 0;
		//(先手なら２段目より下に、後手なら８段目より上に打つ）
		StartDan = (us == BLACK) ? 2 : 1;
		for (suji = 0x10; suji <= 0x90; suji += 0x10) {
			// 二歩チェック
			if (is_double_pawn(us, suji)) continue;
			z = suji + StartDan;
			// 打ち歩詰めもチェック
#define FU_FUNC(z)	\
	if (ban[z] == EMP && !is_pawn_drop_mate(us, z)) {	\
		(mlist++)->move = Move(tmp | To2Move(z));	\
	}
			FU_FUNC(z  )
			FU_FUNC(z+1)
			FU_FUNC(z+2)
			FU_FUNC(z+3)
			FU_FUNC(z+4)
			FU_FUNC(z+5)
			FU_FUNC(z+6)
			FU_FUNC(z+7)
#undef FU_FUNC
		}
	}

	// 香を打つ
	exists = (us == BLACK) ? handS.existKY() : handG.existKY();
	if (exists > 0) {
		tmp  = (us == BLACK) ? Piece2Move(SKY) : Piece2Move(GKY); // From = 0
		//(先手なら２段目より下に、後手なら８段目より上に打つ）
		z = (us == BLACK) ? 0x12 : 0x11;
		for(; z <= 0x99; z += 0x10) {
#define KY_FUNC(z)	\
			if (ban[z] == EMP) {	\
				(mlist++)->move = Move(tmp | To2Move(z));	\
			}
			KY_FUNC(z  )
			KY_FUNC(z+1)
			KY_FUNC(z+2)
			KY_FUNC(z+3)
			KY_FUNC(z+4)
			KY_FUNC(z+5)
			KY_FUNC(z+6)
			KY_FUNC(z+7)
#undef KY_FUNC
		}
	}

	//桂を打つ
	exists = (us == BLACK) ? handS.existKE() : handG.existKE();
	if (exists > 0) {
		//(先手なら３段目より下に、後手なら７段目より上に打つ）
		tmp  = (us == BLACK) ? Piece2Move(SKE) : Piece2Move(GKE); // From = 0
		z = (us == BLACK) ? 0x13 : 0x11;
		for ( ; z <= 0x99; z += 0x10) {
#define KE_FUNC(z)	\
			if (ban[z] == EMP) {	\
				(mlist++)->move = Move(tmp | To2Move(z));	\
			}
			KE_FUNC(z)
			KE_FUNC(z+1)
			KE_FUNC(z+2)
			KE_FUNC(z+3)
			KE_FUNC(z+4)
			KE_FUNC(z+5)
			KE_FUNC(z+6)
#undef KE_FUNC
		}
	}

	// 銀〜飛車は、どこにでも打てる
	const uint32_t koma_start = (us == BLACK) ? SGI : GGI;
	const uint32_t koma_end = (us == BLACK) ? SHI : GHI;
	uint32_t a[4];
	a[0] = (us == BLACK) ? handS.existGI() : handG.existGI();
	a[1] = (us == BLACK) ? handS.existKI() : handG.existKI();
	a[2] = (us == BLACK) ? handS.existKA() : handG.existKA();
	a[3] = (us == BLACK) ? handS.existHI() : handG.existHI();
	for (uint32_t koma = koma_start, i = 0; koma <= koma_end; koma++, i++) {
		if (a[i] > 0) {
			tmp  = Piece2Move(koma); // From = 0
			for (z = 0x11; z <= 0x99; z += 0x10) {
#define GI_FUNC(z)	\
				if (ban[z] == EMP) {	\
					(mlist++)->move = Move(tmp | To2Move(z));	\
				}
				GI_FUNC(z)
				GI_FUNC(z+1)
				GI_FUNC(z+2)
				GI_FUNC(z+3)
				GI_FUNC(z+4)
				GI_FUNC(z+5)
				GI_FUNC(z+6)
				GI_FUNC(z+7)
				GI_FUNC(z+8)
#undef GI_FUNC
			}
		}
	}

#if defined(DEBUG_GENERATE)
	while (top != mlist) {
		Move m = top->move;
		if (!move_is_drop(m)) {
			if (piece_on(Square(move_from(m))) == EMP) {
				assert(false);
			}
		}
		top++;
	}
#endif
	return mlist;
}

//玉の動く手の生成
MoveStack* Position::gen_move_king(const Color us, MoveStack* mlist, int pindir) const
{
	int to;
	Piece koma;
	unsigned int tmp = (us == BLACK) ? From2Move(kingS) | Piece2Move(SOU) :  From2Move(kingG) | Piece2Move(GOU);

#define MoveKB(dir) to = kingS - DIR_##dir;	\
					if (EXIST_EFFECT(effectW[to]) == 0) {	\
						koma = ban[to];		\
						if (koma == EMP || (koma & GOTE)) {		\
							(mlist++)->move = Move(tmp | To2Move(to) | Cap2Move(ban[to]));	\
						}		\
					}
#define MoveKW(dir) to = kingG - DIR_##dir;	\
					if (EXIST_EFFECT(effectB[to]) == 0) {	\
						koma = ban[to];		\
						if (koma != WALL && !(koma & GOTE)) {		\
							(mlist++)->move = Move(tmp | To2Move(to) | Cap2Move(ban[to]));	\
						}		\
					}

	if (us == BLACK) {
		if (pindir == 0) {
			MoveKB(UP)
			MoveKB(UR)
			MoveKB(UL)
			MoveKB(RIGHT)
			MoveKB(LEFT)
			MoveKB(DR)
			MoveKB(DL)
			MoveKB(DOWN)
		} else {
			if (pindir != ABS(DIR_UP)   ) { MoveKB(UP)    }
			if (pindir != ABS(DIR_UR)   ) { MoveKB(UR)    }
			if (pindir != ABS(DIR_UL)   ) { MoveKB(UL)    }
			if (pindir != ABS(DIR_RIGHT)) { MoveKB(RIGHT) }
			if (pindir != ABS(DIR_LEFT) ) { MoveKB(LEFT)  }
			if (pindir != ABS(DIR_DR)   ) { MoveKB(DR)    }
			if (pindir != ABS(DIR_DL)   ) { MoveKB(DL)    }
			if (pindir != ABS(DIR_DOWN) ) { MoveKB(DOWN)  }
		}
	} else {
		if (pindir == 0) {
			MoveKW(UP)
			MoveKW(UR)
			MoveKW(UL)
			MoveKW(RIGHT)
			MoveKW(LEFT)
			MoveKW(DR)
			MoveKW(DL)
			MoveKW(DOWN)
		} else {
			if (pindir != ABS(DIR_UP)   ) { MoveKW(UP)    }
			if (pindir != ABS(DIR_UR)   ) { MoveKW(UR)    }
			if (pindir != ABS(DIR_UL)   ) { MoveKW(UL)    }
			if (pindir != ABS(DIR_RIGHT)) { MoveKW(RIGHT) }
			if (pindir != ABS(DIR_LEFT) ) { MoveKW(LEFT)  }
			if (pindir != ABS(DIR_DR)   ) { MoveKW(DR)    }
			if (pindir != ABS(DIR_DL)   ) { MoveKW(DL)    }
			if (pindir != ABS(DIR_DOWN) ) { MoveKW(DOWN)  }
		}
	}
#undef MoveKB
#undef MoveKW
	return mlist;
}


//fromから動く手の生成
// 盤面のfromにある駒を動かす手を生成する。
// pindir		動けない方向(pinされている)
MoveStack* Position::gen_move_from(const Color us, MoveStack* mlist, int from, int pindir) const
{
	int z_pin = abs(this->pin[from]);
	pindir = abs(pindir);
#define AddMoveM1(teban,dir)     if (pindir != ABS(DIR_ ## dir)) if (z_pin == ABS(DIR_ ## dir)) mlist = add_move##teban(mlist, from, DIR_ ## dir)
#define AddStraightM1(teban,dir) if (pindir != ABS(DIR_ ## dir)) if (z_pin == ABS(DIR_ ## dir)) mlist = add_straight##teban(mlist, from, DIR_ ## dir)
#define AddMoveM2(teban,dir)     if (pindir != ABS(DIR_ ## dir)) mlist = add_move##teban(mlist, from, DIR_ ## dir)
#define AddStraightM2(teban,dir) if (pindir != ABS(DIR_ ## dir)) mlist = add_straight##teban(mlist, from, DIR_ ## dir)
	switch(ban[from]) {
	case SFU:
		if (z_pin) {
			AddMoveM1(B, UP);
		} else if (pindir) {
			AddMoveM2(B, UP);
		} else {
			mlist = add_moveB(mlist, from, DIR_UP);
		}
		break;
	case SKY:
		if (z_pin) {
			AddStraightM1(B, UP);
		} else if (pindir) {
			AddStraightM2(B, UP);
		} else {
			mlist = add_straightB(mlist, from, DIR_UP);
		}
		break;
	case SKE:
		// 桂馬はpinとなる方向がない
		if (z_pin == 0) {
			mlist = add_moveB(mlist, from, DIR_KEUR);
			mlist = add_moveB(mlist, from, DIR_KEUL);
		}
		break;
	case SGI:
		if (z_pin) {
			AddMoveM1(B, UP);
			AddMoveM1(B, UR);
			AddMoveM1(B, UL);
			AddMoveM1(B, DR);
			AddMoveM1(B, DL);
		} else if (pindir) {
			AddMoveM2(B, UP);
			AddMoveM2(B, UR);
			AddMoveM2(B, UL);
			AddMoveM2(B, DR);
			AddMoveM2(B, DL);
		} else {
			mlist = add_moveB(mlist, from, DIR_UP);
			mlist = add_moveB(mlist, from, DIR_UR);
			mlist = add_moveB(mlist, from, DIR_UL);
			mlist = add_moveB(mlist, from, DIR_DR);
			mlist = add_moveB(mlist, from, DIR_DL);
		}
		break;
	case SKI:case STO:case SNY:case SNK:case SNG:
		if (z_pin) {
			AddMoveM1(B, UP);
			AddMoveM1(B, UR);
			AddMoveM1(B, UL);
			AddMoveM1(B, DOWN);
			AddMoveM1(B, RIGHT);
			AddMoveM1(B, LEFT);
		} else if (pindir) {
			AddMoveM2(B, UP);
			AddMoveM2(B, UR);
			AddMoveM2(B, UL);
			AddMoveM2(B, DOWN);
			AddMoveM2(B, RIGHT);
			AddMoveM2(B, LEFT);
		} else {
			mlist = add_moveB(mlist, from, DIR_UP);
			mlist = add_moveB(mlist, from, DIR_UR);
			mlist = add_moveB(mlist, from, DIR_UL);
			mlist = add_moveB(mlist, from, DIR_DOWN);
			mlist = add_moveB(mlist, from, DIR_RIGHT);
			mlist = add_moveB(mlist, from, DIR_LEFT);
		}
		break;
	case SUM:
		if (z_pin) {
			AddMoveM1(B, UP);
			AddMoveM1(B, RIGHT);
			AddMoveM1(B, LEFT);
			AddMoveM1(B, DOWN);
			AddStraightM1(B, UR);
			AddStraightM1(B, UL);
			AddStraightM1(B, DR);
			AddStraightM1(B, DL);
		} else if (pindir) {
			AddMoveM2(B, UP);
			AddMoveM2(B, RIGHT);
			AddMoveM2(B, LEFT);
			AddMoveM2(B, DOWN);
			AddStraightM2(B, UR);
			AddStraightM2(B, UL);
			AddStraightM2(B, DR);
			AddStraightM2(B, DL);
		} else {
			mlist = add_moveB(mlist, from, DIR_UP);
			mlist = add_moveB(mlist, from, DIR_RIGHT);
			mlist = add_moveB(mlist, from, DIR_LEFT);
			mlist = add_moveB(mlist, from, DIR_DOWN);
			mlist = add_straightB(mlist, from, DIR_UR);
			mlist = add_straightB(mlist, from, DIR_UL);
			mlist = add_straightB(mlist, from, DIR_DR);
			mlist = add_straightB(mlist, from, DIR_DL);
		}
		break;
	case SKA:
		if (z_pin) {
			AddStraightM1(B, UR);
			AddStraightM1(B, UL);
			AddStraightM1(B, DR);
			AddStraightM1(B, DL);
		} else if (pindir) {
			AddStraightM2(B, UR);
			AddStraightM2(B, UL);
			AddStraightM2(B, DR);
			AddStraightM2(B, DL);
		} else {
			mlist = add_straightB(mlist, from, DIR_UR);
			mlist = add_straightB(mlist, from, DIR_UL);
			mlist = add_straightB(mlist, from, DIR_DR);
			mlist = add_straightB(mlist, from, DIR_DL);
		}
		break;
	case SRY:
		if (z_pin) {
			AddMoveM1(B, UR);
			AddMoveM1(B, UL);
			AddMoveM1(B, DR);
			AddMoveM1(B, DL);
			AddStraightM1(B, UP);
			AddStraightM1(B, RIGHT);
			AddStraightM1(B, LEFT);
			AddStraightM1(B, DOWN);
		} else if (pindir) {
			AddMoveM2(B, UR);
			AddMoveM2(B, UL);
			AddMoveM2(B, DR);
			AddMoveM2(B, DL);
			AddStraightM2(B, UP);
			AddStraightM2(B, RIGHT);
			AddStraightM2(B, LEFT);
			AddStraightM2(B, DOWN);
		} else {
			mlist = add_moveB(mlist, from, DIR_UR);
			mlist = add_moveB(mlist, from, DIR_UL);
			mlist = add_moveB(mlist, from, DIR_DR);
			mlist = add_moveB(mlist, from, DIR_DL);
			mlist = add_straightB(mlist, from, DIR_UP);
			mlist = add_straightB(mlist, from, DIR_RIGHT);
			mlist = add_straightB(mlist, from, DIR_LEFT);
			mlist = add_straightB(mlist, from, DIR_DOWN);
		}
		break;
	case SHI:
		if (z_pin) {
			AddStraightM1(B, UP);
			AddStraightM1(B, RIGHT);
			AddStraightM1(B, LEFT);
			AddStraightM1(B, DOWN);
		} else if (pindir) {
			AddStraightM2(B, UP);
			AddStraightM2(B, RIGHT);
			AddStraightM2(B, LEFT);
			AddStraightM2(B, DOWN);
		} else {
			mlist = add_straightB(mlist, from, DIR_UP);
			mlist = add_straightB(mlist, from, DIR_RIGHT);
			mlist = add_straightB(mlist, from, DIR_LEFT);
			mlist = add_straightB(mlist, from, DIR_DOWN);
		}
		break;
	case SOU:
		mlist = gen_move_king(us, mlist, pindir);
		break;

	case GFU:
		if (z_pin) {
			AddMoveM1(W, DOWN);
		} else if (pindir) {
			AddMoveM2(W, DOWN);
		} else {
			mlist = add_moveW(mlist, from, DIR_DOWN);
		}
		break;
	case GKY:
		if (z_pin) {
			AddStraightM1(W, DOWN);
		} else if (pindir) {
			AddStraightM2(W, DOWN);
		} else {
			mlist = add_straightW(mlist, from, DIR_DOWN);
		}
		break;
	case GKE:
		// 桂馬はpinとなる方向がない
		if (z_pin == 0) {
			mlist = add_moveW(mlist, from, DIR_KEDR);
			mlist = add_moveW(mlist, from, DIR_KEDL);
		}
		break;
	case GGI:
		if (z_pin) {
			AddMoveM1(W, DOWN);
			AddMoveM1(W, DR);
			AddMoveM1(W, DL);
			AddMoveM1(W, UR);
			AddMoveM1(W, UL);
		} else if (pindir) {
			AddMoveM2(W, DOWN);
			AddMoveM2(W, DR);
			AddMoveM2(W, DL);
			AddMoveM2(W, UR);
			AddMoveM2(W, UL);
		} else {
			mlist = add_moveW(mlist, from, DIR_DOWN);
			mlist = add_moveW(mlist, from, DIR_DR);
			mlist = add_moveW(mlist, from, DIR_DL);
			mlist = add_moveW(mlist, from, DIR_UR);
			mlist = add_moveW(mlist, from, DIR_UL);
		}
		break;
	case GKI:case GTO:case GNY:case GNK:case GNG:
		if (z_pin) {
			AddMoveM1(W, DOWN);
			AddMoveM1(W, DR);
			AddMoveM1(W, DL);
			AddMoveM1(W, UP);
			AddMoveM1(W, RIGHT);
			AddMoveM1(W, LEFT);
		} else if (pindir) {
			AddMoveM2(W, DOWN);
			AddMoveM2(W, DR);
			AddMoveM2(W, DL);
			AddMoveM2(W, UP);
			AddMoveM2(W, RIGHT);
			AddMoveM2(W, LEFT);
		} else {
			mlist = add_moveW(mlist, from, DIR_DOWN);
			mlist = add_moveW(mlist, from, DIR_DR);
			mlist = add_moveW(mlist, from, DIR_DL);
			mlist = add_moveW(mlist, from, DIR_UP);
			mlist = add_moveW(mlist, from, DIR_RIGHT);
			mlist = add_moveW(mlist, from, DIR_LEFT);
		}
		break;
	case GRY:
		if (z_pin) {
			AddMoveM1(W, UR);
			AddMoveM1(W, UL);
			AddMoveM1(W, DR);
			AddMoveM1(W, DL);
			AddStraightM1(W, UP);
			AddStraightM1(W, RIGHT);
			AddStraightM1(W, LEFT);
			AddStraightM1(W, DOWN);
		} else if (pindir) {
			AddMoveM2(W, UR);
			AddMoveM2(W, UL);
			AddMoveM2(W, DR);
			AddMoveM2(W, DL);
			AddStraightM2(W, UP);
			AddStraightM2(W, RIGHT);
			AddStraightM2(W, LEFT);
			AddStraightM2(W, DOWN);
		} else {
			mlist = add_moveW(mlist, from, DIR_UR);
			mlist = add_moveW(mlist, from, DIR_UL);
			mlist = add_moveW(mlist, from, DIR_DR);
			mlist = add_moveW(mlist, from, DIR_DL);
			mlist = add_straightW(mlist, from, DIR_UP);
			mlist = add_straightW(mlist, from, DIR_RIGHT);
			mlist = add_straightW(mlist, from, DIR_LEFT);
			mlist = add_straightW(mlist, from, DIR_DOWN);
		}
		break;
	case GHI:
		if (z_pin) {
			AddStraightM1(W, UP);
			AddStraightM1(W, RIGHT);
			AddStraightM1(W, LEFT);
			AddStraightM1(W, DOWN);
		} else if (pindir) {
			AddStraightM2(W, UP);
			AddStraightM2(W, RIGHT);
			AddStraightM2(W, LEFT);
			AddStraightM2(W, DOWN);
		} else {
			mlist = add_straightW(mlist, from, DIR_UP);
			mlist = add_straightW(mlist, from, DIR_RIGHT);
			mlist = add_straightW(mlist, from, DIR_LEFT);
			mlist = add_straightW(mlist, from, DIR_DOWN);
		}
		break;
	case GUM:
		if (z_pin) {
			AddMoveM1(W, UP);
			AddMoveM1(W, RIGHT);
			AddMoveM1(W, LEFT);
			AddMoveM1(W, DOWN);
			AddStraightM1(W, UR);
			AddStraightM1(W, UL);
			AddStraightM1(W, DR);
			AddStraightM1(W, DL);
		} else if (pindir) {
			AddMoveM2(W, UP);
			AddMoveM2(W, RIGHT);
			AddMoveM2(W, LEFT);
			AddMoveM2(W, DOWN);
			AddStraightM2(W, UR);
			AddStraightM2(W, UL);
			AddStraightM2(W, DR);
			AddStraightM2(W, DL);
		} else {
			mlist = add_moveW(mlist, from, DIR_UP);
			mlist = add_moveW(mlist, from, DIR_RIGHT);
			mlist = add_moveW(mlist, from, DIR_LEFT);
			mlist = add_moveW(mlist, from, DIR_DOWN);
			mlist = add_straightW(mlist, from, DIR_UR);
			mlist = add_straightW(mlist, from, DIR_UL);
			mlist = add_straightW(mlist, from, DIR_DR);
			mlist = add_straightW(mlist, from, DIR_DL);
		}
		break;
	case GKA:
		if (z_pin) {
			AddStraightM1(W, UR);
			AddStraightM1(W, UL);
			AddStraightM1(W, DR);
			AddStraightM1(W, DL);
		} else if (pindir) {
			AddStraightM2(W, UR);
			AddStraightM2(W, UL);
			AddStraightM2(W, DR);
			AddStraightM2(W, DL);
		} else {
			mlist = add_straightW(mlist, from, DIR_UR);
			mlist = add_straightW(mlist, from, DIR_UL);
			mlist = add_straightW(mlist, from, DIR_DR);
			mlist = add_straightW(mlist, from, DIR_DL);
		}
		break;
	case GOU:
		mlist = gen_move_king(us, mlist, pindir);
		break;
	case EMP: case WALL: case PIECE_NONE:
	default:
		break;
	}
#undef AddMoveM
	return mlist;
}

// 取る手(＋歩を成る手)を生成
template<Color us>
MoveStack* Position::generate_capture(MoveStack* mlist) const
{
	int to;
	int from;
	const Color them = (us == BLACK) ? WHITE : BLACK;
	const effect_t *our_effect = (us == BLACK) ? effectB : effectW;
	const effect_t *their_effect = (us == BLACK) ? effectW : effectB;

	int kno;	// 駒番号
	PieceType type;
	effect_t k;
	unsigned long id;
#if defined(DEBUG_GENERATE)
	MoveStack* top = mlist;
#endif

	for (kno = 1; kno <= MAX_KOMANO; kno++) {
		to = knpos[kno];
		if (OnBoard(to)) {
			if (color_of(Piece(knkind[kno])) == them && EXIST_EFFECT(our_effect[to])) {
				// 相手の駒に自分の利きがあれば取れる？(要pin情報の考慮)
				k = our_effect[to] & EFFECT_SHORT_MASK;
				while (k) {
					_BitScanForward(&id, k);
					k &= k-1;
					from = to - NanohaTbl::Direction[id];
					if (pin[from] && abs(pin[from]) != abs(NanohaTbl::Direction[id])) continue;
					type = type_of(ban[from]);
					if (type == OU) {
						// 玉は相手の利きがある駒を取れない
						if (EXIST_EFFECT(their_effect[to]) == 0) {
							mlist->move = cons_move(from, to, ban[from], ban[to], 0);
							mlist++;
						}
					} else if (can_promotion<us>(to) || can_promotion<us>(from)) {
						// 成れる位置？
						if (type == GI) {
							// 銀は成と不成を生成する
							mlist->move = cons_move(from, to, ban[from], ban[to], 1);
							mlist++;
							mlist->move = cons_move(from, to, ban[from], ban[to], 0);
							mlist++;
						} else if (type == FU) {
							// 歩は成のみ生成する
							mlist->move = cons_move(from, to, ban[from], ban[to], 1);
							mlist++;
						} else if (type == KE) {
							// 桂は成を生成し3段目のみ不成を生成する
							mlist->move = cons_move(from, to, ban[from], ban[to], 1);
							mlist++;
							if (is_drop_knight<us>(to)) {
								mlist->move = cons_move(from, to, ban[from], ban[to], 0);
								mlist++;
							}
						} else {
							// 歩桂銀以外の駒(金、成駒)
							mlist->move = cons_move(from, to, ban[from], ban[to], 0);
							mlist++;
						}
					} else {
						// 成れない位置
						mlist->move = cons_move(from, to, ban[from], ban[to]);
						mlist++;
					}
				}
				k = our_effect[to] & EFFECT_LONG_MASK;
				while (k) {
					_BitScanForward(&id, k);
					k &= k-1;
					from = SkipOverEMP(to, -NanohaTbl::Direction[id]);
					if (pin[from] && abs(pin[from]) != abs(NanohaTbl::Direction[id])) continue;
					type = type_of(ban[from]);
					if (type == KA || type == HI) {
						// 角飛は成れるときは成のみ生成する
						if (can_promotion<us>(to) || can_promotion<us>(from)) {
							mlist->move = cons_move(from, to, ban[from], ban[to], 1);
							mlist++;
						} else {
							mlist->move = cons_move(from, to, ban[from], ban[to], 0);
							mlist++;
						}
					} else if (type == KY) {
						if (can_promotion<us>(to)) {
							// 成れる位置
							mlist->move = cons_move(from, to, ban[from], ban[to], 1);
							mlist++;
							// 香は3段目or7段目のときのみ不成を生成する
							if (is_drop_knight<us>(to)) {
								mlist->move = cons_move(from, to, ban[from], ban[to], 0);
								mlist++;
							}
						} else {
							// 成れない位置
							mlist->move = cons_move(from, to, ban[from], ban[to], 0);
							mlist++;
						}
					} else {
						// 角飛香以外の駒(馬、竜)
						mlist->move = cons_move(from, to, ban[from], ban[to]);
						mlist++;
					}
				}
			}
		}
	}
	// 敵陣に飛を成り込む手を生成する
	for (kno = KNS_HI; kno <= KNE_HI; kno++) {
		if (knkind[kno] == make_piece(us, HI)) {
			from = knpos[kno];
			if (OnBoard(from) && can_promotion<us>(from) == false && (pin[from] == 0 || pin[from] == DIR_UP || pin[from] == DIR_DOWN)) {
				const int dir = (us == BLACK) ? DIR_UP : DIR_DOWN;
				to = SkipOverEMP(from, dir);
				while (can_promotion<us>(to -= dir)) {
					mlist->move = cons_move(from, to, ban[from], ban[to], 1);
					mlist++;
				}
			}
		}
	}
	// 歩の成る手を生成
	for (kno = KNS_FU; kno <= KNE_FU; kno++) {
		if (knkind[kno] == make_piece(us, FU)) {
			from = knpos[kno];
			to = (us == BLACK) ? from + DIR_UP : from + DIR_DOWN;
			if (OnBoard(from) && can_promotion<us>(to) && ban[to] == EMP) {
				if (pin[from] == 0 || abs(pin[from]) == 1) {
					mlist->move = cons_move(from, to, ban[from], ban[to], 1);
					mlist++;
				}
			}
		}
	}
#if defined(DEBUG_GENERATE)
	while (top != mlist) {
		Move m = top->move;
		if (!move_is_drop(m)) {
			if (piece_on(Square(move_from(m))) == EMP) {
				assert(false);
			}
		}
		top++;
	}
#endif

	return mlist;
}

// 玉を動かす手の生成(駒を取らない)
// 普通の駒と違い、相手の利きのあるところには動けない
// pindir		動けない方向
MoveStack* Position::gen_king_noncapture(const Color us, MoveStack* mlist, const int pindir) const
{
	int to;
	Piece koma;
	unsigned int tmp = (us == BLACK) ? From2Move(kingS) | Piece2Move(SOU) :  From2Move(kingG) | Piece2Move(GOU);

#define MoveKS(dir) to = kingS - DIR_##dir;	\
					if (EXIST_EFFECT(effectW[to]) == 0) {	\
						koma = ban[to];		\
						if (koma == EMP) {		\
							(mlist++)->move = Move(tmp | To2Move(to) | Cap2Move(ban[to]));	\
						}		\
					}
#define MoveKG(dir) to = kingG - DIR_##dir;	\
					if (EXIST_EFFECT(effectB[to]) == 0) {	\
						koma = ban[to];		\
						if (koma == EMP) {		\
							(mlist++)->move = Move(tmp | To2Move(to) | Cap2Move(ban[to]));	\
						}		\
					}

	if (us == BLACK) {
		if (pindir == 0) {
			MoveKS(UP)
			MoveKS(UR)
			MoveKS(UL)
			MoveKS(RIGHT)
			MoveKS(LEFT)
			MoveKS(DR)
			MoveKS(DL)
			MoveKS(DOWN)
		} else {
			if (pindir != ABS(DIR_UP)   ) { MoveKS(UP)    }
			if (pindir != ABS(DIR_UR)   ) { MoveKS(UR)    }
			if (pindir != ABS(DIR_UL)   ) { MoveKS(UL)    }
			if (pindir != ABS(DIR_RIGHT)) { MoveKS(RIGHT) }
			if (pindir != ABS(DIR_LEFT) ) { MoveKS(LEFT)  }
			if (pindir != ABS(DIR_DR)   ) { MoveKS(DR)    }
			if (pindir != ABS(DIR_DL)   ) { MoveKS(DL)    }
			if (pindir != ABS(DIR_DOWN) ) { MoveKS(DOWN)  }
		}
	} else {
		if (pindir == 0) {
			MoveKG(UP)
			MoveKG(UR)
			MoveKG(UL)
			MoveKG(RIGHT)
			MoveKG(LEFT)
			MoveKG(DR)
			MoveKG(DL)
			MoveKG(DOWN)
		} else {
			if (pindir != ABS(DIR_UP)   ) { MoveKG(UP)    }
			if (pindir != ABS(DIR_UR)   ) { MoveKG(UR)    }
			if (pindir != ABS(DIR_UL)   ) { MoveKG(UL)    }
			if (pindir != ABS(DIR_RIGHT)) { MoveKG(RIGHT) }
			if (pindir != ABS(DIR_LEFT) ) { MoveKG(LEFT)  }
			if (pindir != ABS(DIR_DR)   ) { MoveKG(DR)    }
			if (pindir != ABS(DIR_DL)   ) { MoveKG(DL)    }
			if (pindir != ABS(DIR_DOWN) ) { MoveKG(DOWN)  }
		}
	}
#undef MoveKS
#undef MoveKG
	return mlist;
}

// 盤上の駒を動かす手のうち generate_capture() で生成する手を除いて生成する(動かす手で取らない手(−歩を成る手)を生成)
template <Color us>
MoveStack* Position::generate_non_capture(MoveStack* mlist) const
{
	int kn;
	int from;
	MoveStack* p = mlist;
#if defined(DEBUG_GENERATE)
	MoveStack* top = mlist;
#endif

	from = sq_king<us>();	// 玉
	if (from) mlist = gen_king_noncapture(us, mlist);	// 詰将棋など玉がない時を顧慮
	for (kn = KNS_HI; kn <= KNE_FU; kn++) {
		from = knpos[kn];
		if (OnBoard(from)) {
			if (color_of(Piece(knkind[kn])) == us) {
				// 手番のとき
				mlist = gen_move_from(us, mlist, from);
			}
		}
	}

	// generate_capture()で生成される手を除く
	MoveStack *last = mlist;
	for (mlist = p; mlist < last; mlist++) {
		Move &tmp = mlist->move;
		// 取る手はほぼ生成済み
		//   取る手で成れるのに成らない手は生成していない
		if (move_captured(tmp) != EMP) {
			if (is_promotion(tmp)) continue;
			// ここで、取る手&&成らない手になっている。
			PieceType pt = move_ptype(tmp);
			switch (pt) {
			case FU:
				if (can_promotion<us>(move_to(tmp))) break;
				continue;
			case KE:
			case GI:
			case KI:
			case OU:
			case TO:
			case NY:
			case NK:
			case NG:
			case UM:
			case RY:
				continue;
			case KY:
				//   香車は3段目(7段目)は生成しているので、除外する(対象は2段目(8段目)のみ)
				if (((us == BLACK && (move_to(tmp) & 0x0F) != 2) || (us == WHITE && (move_to(tmp) & 0x0F) != 8))) continue;
				break;
			case KA:
			case HI:
				// 角飛は成らない手を生成していないので、成れるのに成らない手はすべて対象にする
				if (!can_promotion<us>(move_to(tmp)) && !can_promotion<us>(move_from(tmp))) continue;
				break;
			case PIECE_TYPE_NONE:
			default:
				print_csa(tmp);
				MYABORT();
				break;
			}
		}
		// 飛車が敵陣に成り込む手は生成済み
		if (move_ptype(tmp) == HI && is_promotion(tmp) && !can_promotion<us>(move_from(tmp)) && can_promotion<us>(move_to(tmp))) continue;
		// 歩の成る手は生成済み
		if (move_ptype(tmp) == FU && is_promotion(tmp)) continue;
		if (mlist != p) (p++)->move = tmp;
		else p++;
	}

#if defined(DEBUG_GENERATE)
	mlist = p;
	while (top != mlist) {
		Move m = top->move;
		if (!move_is_drop(m)) {
			if (piece_on(Square(move_from(m))) == EMP) {
				assert(false);
			}
		}
		top++;
	}
#endif

	return gen_drop<us>(p);
}

// 王手回避手の生成
template<Color us>
MoveStack* Position::generate_evasion(MoveStack* mlist) const
{
	const effect_t efft = (us == BLACK) ? effectW[kingS] & (EFFECT_LONG_MASK | EFFECT_SHORT_MASK) : effectB[kingG] & (EFFECT_LONG_MASK | EFFECT_SHORT_MASK);
#if defined(DEBUG_GENERATE)
	MoveStack* top = mlist;
#endif

	if ((efft & (efft - 1)) != 0) {
		// 両王手(利きが2以上)の場合は玉を動かすしかない
		return gen_move_king(us, mlist);
	} else {
		Square ksq = (us == BLACK) ? Square(kingS) : Square(kingG);
		unsigned long id = 0;	// 初期化不要だが warning が出るため0を入れる
		int check;	// 王手をかけている駒の座標
		if (efft & EFFECT_SHORT_MASK) {
			// 跳びのない利きによる王手 → 回避手段：王手している駒を取る、玉を動かす
			_BitScanForward(&id, efft);
			check = ksq - NanohaTbl::Direction[id];
			//王手駒を取る
			mlist = gen_move_to(us, mlist, check);
			//玉を動かす
			mlist = gen_move_king(us, mlist);
		} else {
			// 跳び利きによる王手 → 回避手段：王手している駒を取る、玉を動かす、合駒
			_BitScanForward(&id, efft);
			id -= EFFECT_LONG_SHIFT;
			check = SkipOverEMP(ksq, -NanohaTbl::Direction[id]);
			//王手駒を取る
			mlist = gen_move_to(us, mlist, check);
			//玉を動かす
			mlist = gen_move_king(us, mlist);
			//合駒をする手を生成する
			int sq;
			for (sq = ksq - NanohaTbl::Direction[id]; ban[sq] == EMP; sq -= NanohaTbl::Direction[id]) {
				mlist = gen_move_to(us, mlist, sq);
			}
			for (sq = ksq - NanohaTbl::Direction[id]; ban[sq] == EMP; sq -= NanohaTbl::Direction[id]) {
				mlist = gen_drop_to(us, mlist, sq);  //駒を打つ合;
			}
		}
	}
#if defined(DEBUG_GENERATE)
	while (top != mlist) {
		Move m = top->move;
		if (!move_is_drop(m)) {
			if (piece_on(Square(move_from(m))) == EMP) {
				assert(false);
			}
		}
		top++;
	}
#endif

	return mlist;
}

template<Color us>
MoveStack* Position::generate_non_evasion(MoveStack* mlist) const
{
	int z;

	// 盤上の駒を動かす
	int kn;
	z = (us == BLACK) ? knpos[1] : knpos[2];
	if (z) mlist = gen_move_king(us, mlist);
	for (kn = KNS_HI; kn <= KNE_FU; kn++) {
		z = knpos[kn];
		if (OnBoard(z)) {
			Piece kind = ban[z];
			if (color_of(kind) == us) {
				mlist = gen_move_from(us, mlist, z);
			}
		}
	}
	mlist = gen_drop<us>(mlist);
	return mlist;
}

// 機能：勝ち宣言できるかどうか判定する
//
// 引数：手番
//
// 戻り値
//   true：勝ち宣言できる
//   false：勝ち宣言できない
//
bool Position::IsKachi(const Color us) const
{
	// 入玉宣言勝ちの条件について下に示す。
	// --
	// (a) 宣言側の手番である。
	// (b) 宣言側の玉が敵陣三段目以内に入っている。
	// (c) 宣言側が(大駒5点小駒1点の計算で)
	//   先手の場合28点以上の持点がある。
	//   後手の場合27点以上の持点がある。
	//   点数の対象となるのは、宣言側の持駒と敵陣三段目以内に存在する玉を除く宣言側の駒のみである
	// (d) 宣言側の敵陣三段目以内の駒は、玉を除いて10枚以上存在する。
	// (e) 宣言側の玉に王手がかかっていない。(詰めろや必死であることは関係ない)
	// (f) 宣言側の持ち時間が残っている。(切れ負けの場合)

	int suji;
	int dan;
	int maisuu = 0;
	unsigned int point = 0;
	// 条件(a)
	if (us == BLACK) {
		// 条件(b) 判定
		if ((kingS & 0x0F) > 3) return false;
		// 条件(e)
		if (EXIST_EFFECT(effectW[kingS])) return false;
		// 条件(c)(d) 判定
		for (suji = 0x10; suji <= 0x90; suji += 0x10) {
			for (dan = 1; dan <= 3; dan++) {
				Piece piece = Piece(ban[suji+dan] & ~PROMOTED);		// 玉は0になるのでカウント外
				if (piece != EMP && !(piece & GOTE)) {
					if (piece == SHI || piece == SKA) point += 5;
					else point++;
					maisuu++;
				}
			}
		}
		// 条件(d) 判定
		if (maisuu < 10) return false;
		point += handS.getFU() + handS.getKY() + handS.getKE() + handS.getGI() + handS.getKI();
		point += 5 * handS.getKA();
		point += 5 * handS.getHI();
		// 条件(c) 判定 (先手28点以上、後手27点以上)
		if (point < 28) return false;
	} else {
		// 条件(b) 判定
		if ((kingG & 0x0F) < 7) return false;
		// 条件(e)
		if (EXIST_EFFECT(effectB[kingG])) return false;
		// 条件(c)(d) 判定
		for (suji = 0x10; suji <= 0x90; suji += 0x10) {
			for (dan = 7; dan <= 9; dan++) {
				Piece piece = Piece(ban[suji+dan] & ~PROMOTED);
				if (piece == (GOU & ~PROMOTED)) continue;
				if (piece & GOTE) {
					if (piece == GHI || piece == GKA) point += 5;
					else point++;
					maisuu++;
				}
			}
		}
		// 条件(d) 判定
		if (maisuu < 10) return false;
		point += handG.getFU() + handG.getKY() + handG.getKE() + handG.getGI() + handG.getKI();
		point += 5 * handG.getKA();
		point += 5 * handG.getHI();
		// 条件(c) 判定 (先手28点以上、後手27点以上)
		if (point < 27) return false;
	}

	return true;
}

namespace {
//
//  ハフマン化
//           盤上(6 + α)  持駒(5 + β)
//            α(S/G + Promoted)、β(S/G)
//    空     xxxxx0 + 0    (none)
//    歩     xxxx01 + 2    xxxx0 + 1
//    香     xx0011 + 2    xx001 + 1
//    桂     xx1011 + 2    xx101 + 1
//    銀     xx0111 + 2    xx011 + 1
//    金     x01111 + 1    x0111 + 1
//    角     011111 + 2    01111 + 1
//    飛     111111 + 2    11111 + 1
//
static const struct HuffmanBoardTBL {
	int code;
	int bits;
} HB_tbl[] = {
	{0x00, 1},	// EMP
	{0x01, 4},	// SFU
	{0x03, 6},	// SKY
	{0x0B, 6},	// SKE
	{0x07, 6},	// SGI
	{0x0F, 6},	// SKI
	{0x1F, 8},	// SKA
	{0x3F, 8},	// SHI
	{0x00, 0},	// SOU
	{0x05, 4},	// STO
	{0x13, 6},	// SNY
	{0x1B, 6},	// SNK
	{0x17, 6},	// SNG
	{0x00,-1},	// ---
	{0x5F, 8},	// SUM
	{0x7F, 8},	// SRY
	{0x00,-1},	// ---
	{0x09, 4},	// GFU
	{0x23, 6},	// GKY
	{0x2B, 6},	// GKE
	{0x27, 6},	// GGI
	{0x2F, 6},	// GKI
	{0x9F, 8},	// GKA
	{0xBF, 8},	// GHI
	{0x00, 0},	// GOU
	{0x0D, 4},	// GTO	// 間違い
	{0x33, 6},	// GNY
	{0x3B, 6},	// GNK
	{0x37, 6},	// GNG
	{0x00,-1},	// ---
	{0xDF, 8},	// GUM
	{0xFF, 8},	// GRY
	{0x00,-1},	// ---
};

static const struct HuffmanHandTBL {
	int code;
	int bits;
} HH_tbl[] = {
	{0x00,-1},	// EMP
	{0x00, 2},	// SFU
	{0x01, 4},	// SKY
	{0x05, 4},	// SKE
	{0x03, 4},	// SGI
	{0x07, 5},	// SKI
	{0x0F, 6},	// SKA
	{0x1F, 6},	// SHI
	{0x00,-1},	// SOU
	{0x00,-1},	// STO
	{0x00,-1},	// SNY
	{0x00,-1},	// SNK
	{0x00,-1},	// SNG
	{0x00,-1},	// ---
	{0x00,-1},	// SUM
	{0x00,-1},	// SRY
	{0x00,-1},	// ---
	{0x02, 2},	// GFU
	{0x09, 4},	// GKY
	{0x0D, 4},	// GKE
	{0x0B, 4},	// GGI
	{0x17, 5},	// GKI
	{0x2F, 6},	// GKA
	{0x3F, 6},	// GHI
	{0x00,-1},	// GOU
	{0x00,-1},	// GTO
	{0x00,-1},	// GNY
	{0x00,-1},	// GNK
	{0x00,-1},	// GNG
	{0x00,-1},	// ---
	{0x00,-1},	// GUM
	{0x00,-1},	// GRY
	{0x00,-1},	// ---
};

//
// 引数
//   const int start_bit;	// 記録開始bit位置
//   const int bits;		// ビット数(幅)
//   const int data;		// 記録するデータ
//   unsigned char buf[];	// 符号化したデータを記録するバッファ
//   const int size;		// バッファサイズ
//
// 戻り値
//   取り出したデータ
//
int set_bit(const int start_bit, const int bits, const int data, unsigned char buf[], const int size)
{
	static const int mask_tbl[] = {
		0x0000, 0x0001, 0x0003, 0x0007, 0x000F, 0x001F, 0x003F, 0x007F, 0x00FF
	};
	if (start_bit < 0) return -1;
	if (bits <= 0 || bits > 8) return -1;
	if (start_bit + bits > 8*size) return -2;
	if ((data & mask_tbl[bits]) != data) return -3;

	const int n = start_bit / 8;
	const int shift = start_bit % 8;
	buf[n] |= (data << shift);
	if (shift + bits > 8) {
		buf[n+1] = (data >> (8 - shift));
	}
	return start_bit + bits;
}
};

// 機能：局面をハフマン符号化する(定跡ルーチン用)
//
// 引数
//   const int SorG;		// 手番
//   unsigned char buf[];	// 符号化したデータを記録するバッファ
//
// 戻り値
//   マイナス：エラー
//   正の値：エンコードしたときのビット数
//
int Position::EncodeHuffman(unsigned char buf[32]) const
{
	const int KingS = (((kingS >> 4)-1) & 0x0F)*9+(kingS & 0x0F);
	const int KingG = (((kingG >> 4)-1) & 0x0F)*9+(kingG & 0x0F);
	const int size = 32;	// buf[] のサイズ

	int start_bit = 0;

	if (kingS == 0 || kingG == 0) {
		// Error!
		return -1;
	}
///	printf("KingS=%d\n", KingS);
///	printf("KingG=%d\n", KingG);

	memset(buf, 0, size);

	// 手番を符号化
	start_bit = set_bit(start_bit, 1, side_to_move(), buf, size);
	// 玉の位置を符号化
	start_bit = set_bit(start_bit, 7, KingS,               buf, size);
	start_bit = set_bit(start_bit, 7, KingG,               buf, size);

	// 盤上のデータを符号化
	int suji, dan;
	int piece;
	for (suji = 0x10; suji <= 0x90; suji += 0x10) {
		for (dan = 1; dan <= 9; dan++) {
			piece = ban[suji + dan];
			if (piece < EMP || piece > GRY) {
				// Error!
				exit(1);
			}
			if (HB_tbl[piece].bits < 0) {
				// Error!
				exit(1);
			}
			if (HB_tbl[piece].bits == 0) {
				// 玉は別途
				continue;
			}
			start_bit = set_bit(start_bit, HB_tbl[piece].bits, HB_tbl[piece].code, buf, size);
		}
	}

	// 持駒を符号化
	unsigned int i, n;
#define EncodeHand(SG,KOMA)			\
		piece = SG ## KOMA;			\
		n = hand ## SG.get ## KOMA();	\
		for (i = 0; i < n; i++) {	\
			start_bit = set_bit(start_bit, HH_tbl[piece].bits, HH_tbl[piece].code, buf, size);	\
		}

	EncodeHand(G,HI)
	EncodeHand(G,KA)
	EncodeHand(G,KI)
	EncodeHand(G,GI)
	EncodeHand(G,KE)
	EncodeHand(G,KY)
	EncodeHand(G,FU)

	EncodeHand(S,HI)
	EncodeHand(S,KA)
	EncodeHand(S,KI)
	EncodeHand(S,GI)
	EncodeHand(S,KE)
	EncodeHand(S,KY)
	EncodeHand(S,FU)

#undef EncodeHand

	return start_bit;
}

// インスタンス化.
template MoveStack* Position::generate_capture<BLACK>(MoveStack* mlist) const;
template MoveStack* Position::generate_capture<WHITE>(MoveStack* mlist) const;
template MoveStack* Position::generate_non_capture<BLACK>(MoveStack* mlist) const;
template MoveStack* Position::generate_non_capture<WHITE>(MoveStack* mlist) const;
template MoveStack* Position::generate_evasion<BLACK>(MoveStack* mlist) const;
template MoveStack* Position::generate_evasion<WHITE>(MoveStack* mlist) const;
template MoveStack* Position::generate_non_evasion<BLACK>(MoveStack* mlist) const;
template MoveStack* Position::generate_non_evasion<WHITE>(MoveStack* mlist) const;

