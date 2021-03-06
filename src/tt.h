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

#if !defined(TT_H_INCLUDED)
#define TT_H_INCLUDED

#include <iostream>

#include "move.h"
#include "types.h"


/// The TTEntry is the class of transposition table entries
///
/// A TTEntry needs 128 bits to be stored
///
/// bit  0-31: key
/// bit 32-63: data
/// bit 64-79: value
/// bit 80-95: depth
/// bit 96-111: static value
/// bit 112-127: margin of static value
///
/// the 32 bits of the data field are so defined
///
/// bit  0-15: move
/// bit 16-20: not used
/// bit 21-22: value type
/// bit 23-31: generation

/// なのはでの必要bit数
/// The TTEntry is the class of transposition table entries
///
/// A TTEntry needs 192 bits to be stored
///
/// bit   0- 15: depth : 16bits
/// bit  16- 63: key : 48bits
/// bit  64- 95: move : 32bits
/// bit  96-125: hand : 30bits
/// bit 126-127: value type : 2bits
/// bit 128-143: generation : 16bits
/// bit 144-159: value : 16bits
/// bit 160-175: static value : 16bits
/// bit 176-191: margin of static value : 16bits

class TTEntry {

public:
#if defined(NANOHA)
	void save(uint64_t k, uint32_t h, Value v, ValueType t, Depth d, Move m, int g, Value statV, Value statM) {

		key_depth    = (k & ~UINT64_C(0xFFFF)) | (d & 0xFFFF);
		move32       = m;
		hand30       = (h & 0x3FFFFFFF) | (t << 30);
		generation16 = uint16_t(g);
		value16      = int16_t(v);
		staticValue  = int16_t(statV);
		staticMargin = int16_t(statM);
	}
	void set_generation(int g) { generation16 = uint16_t(g); }

	uint64_t key() const              { return (key_depth & ~UINT64_C(0xFFFF)); }
	uint32_t hand() const             { return (hand30 & 0x3FFFFFFF); }
	Depth depth() const               { return Depth((int16_t)(key_depth & 0xFFFF)); }
	Move move() const                 { return Move(move32); }
	Value value() const               { return Value(value16); }
	ValueType type() const            { return ValueType((hand30 >> 30) & 0x0003); }
	int generation() const            { return int(generation16); }
	Value static_value() const        { return Value(staticValue); }
	Value static_value_margin() const { return Value(staticMargin); }
#else
	void save(uint32_t k, Value v, ValueType t, Depth d, Move m, int g, Value statV, Value statM) {

		key32        = (uint32_t)k;
		move16       = (uint16_t)m;
		valueType    = (uint8_t)t;
		generation8  = (uint8_t)g;
		value16      = (int16_t)v;
		depth16      = (int16_t)d;
		staticValue  = (int16_t)statV;
		staticMargin = (int16_t)statM;
	}
	void set_generation(int g) { generation8 = (uint8_t)g; }

	uint32_t key() const              { return key32; }
	Depth depth() const               { return (Depth)depth16; }
	Move move() const                 { return (Move)move16; }
	Value value() const               { return (Value)value16; }
	ValueType type() const            { return (ValueType)valueType; }
	int generation() const            { return (int)generation8; }
	Value static_value() const        { return (Value)staticValue; }
	Value static_value_margin() const { return (Value)staticMargin; }
#endif

private:
#if defined(NANOHA)
	uint64_t key_depth;
	uint32_t hand30;
	uint32_t move32;
	uint16_t generation16;
	int16_t value16, staticValue, staticMargin;
#else
	uint32_t key32;
	uint16_t move16;
	uint8_t valueType, generation8;
	int16_t value16, depth16, staticValue, staticMargin;
#endif
};


/// This is the number of TTEntry slots for each cluster
// TODO:要検証:将棋で持ち駒を別に算出する場合は、局面hash一致がするパターンが多くなるので、4から増やしたほうがいい？
const int ClusterSize = 4;


/// TTCluster consists of ClusterSize number of TTEntries. Size of TTCluster
/// must not be bigger than a cache line size. In case it is less, it should
/// be padded to guarantee always aligned accesses.

struct TTCluster {
	TTEntry data[ClusterSize];
};


/// The transposition table class. This is basically just a huge array containing
/// TTCluster objects, and a few methods for writing and reading entries.

class TranspositionTable {

	TranspositionTable(const TranspositionTable&);
	TranspositionTable& operator=(const TranspositionTable&);

public:
	TranspositionTable();
	~TranspositionTable();
	void set_size(size_t mbSize);
	void clear();
#if defined(NANOHA)
	void store(const Key posKey, uint32_t h, Value v, ValueType type, Depth d, Move m, Value statV, Value kingD);
	TTEntry* probe(const Key posKey, uint32_t h) const;
#else
	void store(const Key posKey, Value v, ValueType type, Depth d, Move m, Value statV, Value kingD);
	TTEntry* probe(const Key posKey) const;
#endif
	void new_search();
	TTEntry* first_entry(const Key posKey) const;
	void refresh(const TTEntry* tte) const;

private:
	size_t size;
	TTCluster* entries;
#if defined(NANOHA)
	uint16_t generation; // Size must be not bigger then TTEntry::generation8
#else
	uint8_t generation; // Size must be not bigger then TTEntry::generation8
#endif
};

extern TranspositionTable TT;


/// TranspositionTable::first_entry() returns a pointer to the first entry of
/// a cluster given a position. The lowest order bits of the key are used to
/// get the index of the cluster.

inline TTEntry* TranspositionTable::first_entry(const Key posKey) const {

	return entries[((uint32_t)posKey) & (size - 1)].data;
}


/// TranspositionTable::refresh() updates the 'generation' value of the TTEntry
/// to avoid aging. Normally called after a TT hit.

inline void TranspositionTable::refresh(const TTEntry* tte) const {

	const_cast<TTEntry*>(tte)->set_generation(generation);
}


/// A simple fixed size hash table used to store pawns and material
/// configurations. It is basically just an array of Entry objects.
/// Without cluster concept or overwrite policy.
#if !defined(NANOHA)
template<class Entry, int HashSize>
struct SimpleHash {

	typedef SimpleHash<Entry, HashSize> Base;

	void init() {

		if (entries)
			return;

		entries = new (std::nothrow) Entry[HashSize];
		if (!entries)
		{
			std::cerr << "Failed to allocate " << HashSize * sizeof(Entry)
			          << " bytes for hash table." << std::endl;
			exit(EXIT_FAILURE);
		}
		memset(entries, 0, HashSize * sizeof(Entry));
	}

	virtual ~SimpleHash() { delete [] entries; }

	Entry* probe(Key key) const { return entries + ((uint32_t)key & (HashSize - 1)); }
	void prefetch(Key key) const { ::prefetch((char*)probe(key)); }

protected:
	Entry* entries;
};
#endif

#endif // !defined(TT_H_INCLUDED)
