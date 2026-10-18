#include "../config.h"

#include "../misc.h"
#include "../position.h"

#include <sstream>
#include <fstream>
#include <cstring>	

using namespace std;
namespace YaneuraOu {


struct BitStream
{
	void  set_data(u8* data_) { data = data_; reset(); }

	u8* get_data() const { return data; }

	int get_cursor() const { return bit_cursor; }

	void reset() { bit_cursor = 0; }

	FORCE_INLINE void write_one_bit(int b)
	{
	if (b)
		data[bit_cursor / 8] |= 1 << (bit_cursor & 7);

	++bit_cursor;
	}

	FORCE_INLINE int read_one_bit()
	{
	int b = (data[bit_cursor / 8] >> (bit_cursor & 7)) & 1;
	++bit_cursor;

	return b;
	}

	void write_n_bit(int d, int n)
	{
	for (int i = 0; i < n; ++i)
		write_one_bit(d & (1 << i));
	}

	int read_n_bit(int n)
	{
	int result = 0;
	for (int i = 0; i < n; ++i)
		result |= read_one_bit() ? (1 << i) : 0;

	return result;
	}

private:
	int bit_cursor;

	u8* data;
};



struct HuffmanedPiece
{
	int code; 
	int bits; 
};

HuffmanedPiece huffman_table[] =
{
	{0x00,1}, 
	{0x01,2}, 
	{0x03,4}, 
	{0x0b,4}, 
	{0x07,4}, 
	{0x1f,6}, 
	{0x3f,6}, 
	{0x0f,5}, 
};

HuffmanedPiece huffman_table_piecebox[] =
{
	{0x00,1}, 
	{0x02,2}, 
	{0x09,4}, 
	{0x0d,4}, 
	{0x0b,4}, 
	{0x2f,6}, 
	{0x3f,6}, 
	{0x1b,5}, 
};


struct SfenPacker
{
	void pack(const Position& pos)
	{

		constexpr PieceType to_apery_pieces[]     = { NO_PIECE_TYPE , PAWN, LANCE, KNIGHT, SILVER, GOLD, BISHOP , ROOK };

		int32_t hp_count[8] =
		{
			0,
			18, 4, 4, 4,
			2, 2, 4
		};

		memset(data, 0, 32 );
		stream.set_data(data);

		stream.write_one_bit((int)(pos.side_to_move()));

		for(auto c : COLOR)
			stream.write_n_bit(pos.square<KING>(c), 7);

		for (auto sq : SQ)
		{
			Piece pc = pos.piece_on(sq);
			if (type_of(pc) == KING)
				continue;
			write_board_piece_to_stream(pc);

			hp_count[type_of(raw_of(pc))]--;
		}

		for (auto c : COLOR)
			for (PieceType pr = PAWN; pr < KING; ++pr)
			{
				PieceType pr2 = to_apery_pieces[pr];

				int n = hand_count(pos.hand_of(c), pr2);

				for (int i = 0; i < n; ++i)
					write_hand_piece_to_stream(make_piece(c, pr2));

				hp_count[pr2] -= n;
			}

		for (PieceType pr = PAWN ; pr < KING ; ++pr)
		{
			PieceType pr2 = to_apery_pieces[pr];

			int n = hp_count[pr2];

			for (int i = 0; i < n ; ++i)
				write_piecebox_piece_to_stream(pr2);
		}


		ASSERT_LV3(stream.get_cursor() == 256);
	}

	void pack_rawdata(const Piece board[81], const Hand hand[2], Color turn)
	{
		constexpr PieceType to_apery_pieces[]     = { NO_PIECE_TYPE , PAWN, LANCE, KNIGHT, SILVER, GOLD, BISHOP , ROOK };

		int32_t hp_count[8] =
		{
			0,
			18, 4, 4, 4,
			2, 2, 4
		};

		memset(data, 0, 32 );
		stream.set_data(data);

		stream.write_one_bit((int)turn);

		for (auto c : COLOR)
		{
			Square king_sq = SQ_NB;
			for (auto sq : SQ)
			{
				if (board[sq] == make_piece(c, KING))
				{
					king_sq = sq;
					break;
				}
			}
			stream.write_n_bit(king_sq, 7);
		}

		for (auto sq : SQ)
		{
			Piece pc = board[sq];
			if (type_of(pc) == KING)
				continue;
			write_board_piece_to_stream(pc);

			hp_count[type_of(raw_of(pc))]--;
		}

		for (auto c : COLOR)
			for (PieceType pr = PAWN; pr < KING; ++pr)
			{
				PieceType pr2 = to_apery_pieces[pr];

				int n = hand_count(hand[c], pr2);

				for (int i = 0; i < n; ++i)
					write_hand_piece_to_stream(make_piece(c, pr2));

				hp_count[pr2] -= n;
			}

		for (PieceType pr = PAWN ; pr < KING ; ++pr)
		{
			PieceType pr2 = to_apery_pieces[pr];

			int n = hp_count[pr2];

			for (int i = 0; i < n ; ++i)
				write_piecebox_piece_to_stream(pr2);
		}

		ASSERT_LV3(stream.get_cursor() == 256);
	}

	void unpack_rawdata(Piece board[81], Hand hand[2], Color& turn)
	{
		stream.set_data(data);

		memset(board, 0, sizeof(Piece) * 81);
		hand[BLACK] = hand[WHITE] = HAND_ZERO;

		turn = (Color)stream.read_one_bit();

		for (auto c : COLOR)
		{
			Square king_sq = (Square)stream.read_n_bit(7);
			if (king_sq < SQ_NB)
				board[king_sq] = make_piece(c, KING);
		}

		for (auto sq : SQ)
		{
			if (type_of(board[sq]) == KING)
				continue;

			board[sq] = read_board_piece_from_stream();

			ASSERT_LV3(stream.get_cursor() <= 256);
		}

		while (stream.get_cursor() != 256)
		{
			auto pc = read_hand_piece_from_stream();

			if (is_promoted(pc))
				continue;

			add_hand(hand[(int)color_of(pc)], type_of(pc));
		}
	}

	std::string unpack()
	{
		Piece board[81];
		Hand hand[2];
		Color turn;
		unpack_rawdata(board, hand, turn);


		return Position::sfen_from_rawdata(board, hand, turn, 0);
	}

	u8 *data; 


	BitStream stream;

	void write_board_piece_to_stream(Piece pc)
	{
		PieceType pr = raw_type_of(pc);
		auto c = huffman_table[pr];
		stream.write_n_bit(c.code, c.bits);
 
		if (pc == NO_PIECE)
			return;

		if (pr!=GOLD)
			stream.write_one_bit((PIECE_PROMOTE & pc) ? 1 : 0);

		stream.write_one_bit(color_of(pc));
	}

	void write_hand_piece_to_stream(Piece pc)
	{
		ASSERT_LV3(pc != NO_PIECE);

		PieceType pr = raw_type_of(pc);
		auto c = huffman_table[pr];
		stream.write_n_bit(c.code >> 1, c.bits - 1);

		if (pr != GOLD)
			stream.write_one_bit(false);

		stream.write_one_bit(color_of(pc));
	}

	void write_piecebox_piece_to_stream(PieceType pr)
	{
		ASSERT_LV3(pr != NO_PIECE_TYPE);

		auto c = huffman_table_piecebox[pr];
		stream.write_n_bit(c.code , c.bits);


		if (pr != GOLD)
			stream.write_one_bit(false);
	}

	Piece read_board_piece_from_stream()
	{
		PieceType pr = NO_PIECE_TYPE;
		int code = 0, bits = 0;
		while (true)
		{
			code |= stream.read_one_bit() << bits;
			++bits;

			ASSERT_LV3(bits <= 6);

			for (pr = NO_PIECE_TYPE; pr < KING; ++pr)
			if (huffman_table[pr].code == code
				&& huffman_table[pr].bits == bits)
				goto Found;
		}
		Found:;
		if (pr == NO_PIECE_TYPE)
			return NO_PIECE;

		bool promote = (pr == GOLD) ? false : stream.read_one_bit();

		Color c = (Color)stream.read_one_bit();
    
		return make_piece(c, pr + (promote ? PIECE_TYPE_PROMOTE : NO_PIECE_TYPE));
	}

	Piece read_hand_piece_from_stream()
	{
		PieceType pr = NO_PIECE_TYPE;
		int code = 0, bits = 0;
		while (true)
		{
			code |= stream.read_one_bit() << bits;
			++bits;

			ASSERT_LV3(bits <= 6);

			for (pr = PAWN; pr < KING; ++pr)
				if ((huffman_table[pr].code >> 1) == code
					&& (huffman_table[pr].bits -1) == bits)
				goto Found;
		}
	Found:;
		ASSERT_LV3(pr != NO_PIECE_TYPE);

		if (pr != GOLD)
		{
			bool promote = stream.read_one_bit();
			if (promote)
				pr = PieceType(pr | PIECE_PROMOTE);
		}

		Color c = (Color)stream.read_one_bit();

		return make_piece(c, pr);
	}
};


PackedSfen PackedSfen::flipped() const
{
	PackedSfen result;

	Piece board[81];
	Hand hand[2];
	Color turn;

	SfenPacker unpacker;
	unpacker.data = const_cast<u8*>(data);
	unpacker.unpack_rawdata(board, hand, turn);

	Piece flipped_board[81];
	memset(flipped_board, 0, sizeof(flipped_board));

	for (auto sq : SQ)
	{
		Piece pc = board[sq];
		if (pc == NO_PIECE)
			continue;
		flipped_board[Flip(sq)] = Piece(pc ^ PIECE_WHITE);
	}

	Hand flipped_hand[2];
	flipped_hand[BLACK] = hand[WHITE];
	flipped_hand[WHITE] = hand[BLACK];

	SfenPacker packer;
	packer.data = result.data;
	packer.pack_rawdata(flipped_board, flipped_hand, ~turn);

	return result;
}

void PackedSfen::flip()
{
	*this = flipped();
}

Tools::Result Position::set_from_packed_sfen(const PackedSfen& sfen , StateInfo * si, bool mirror , int gamePly_ )
{
	SfenPacker packer;
	auto& stream = packer.stream;
	stream.set_data((u8*)&sfen);

	std::memset(static_cast<void*>(this), 0, sizeof(Position));
	std::memset(static_cast<void*>(si), 0, sizeof(StateInfo));
	st = si;

	sideToMove = (Color)stream.read_one_bit();

	#if defined(USE_EVAL_LIST)

	evalList.clear();

	PieceNumber piece_no_count[KING] = { PIECE_NUMBER_ZERO,PIECE_NUMBER_PAWN,PIECE_NUMBER_LANCE,PIECE_NUMBER_KNIGHT,
		PIECE_NUMBER_SILVER, PIECE_NUMBER_BISHOP, PIECE_NUMBER_ROOK,PIECE_NUMBER_GOLD };
	#endif

	kingSquare[BLACK] = kingSquare[WHITE] = SQ_NB;

	if (mirror)
	{
		for (auto c : COLOR)
			board[Mir((Square)stream.read_n_bit(7))] = make_piece(c, KING);
	}
	else
	{
		for (auto c : COLOR)
			board[stream.read_n_bit(7)] = make_piece(c, KING);
	}

	for (auto sq : SQ)
	{
		if (mirror)
			sq = Mir(sq);

		Piece pc;
		if (type_of(board[sq]) != KING)
		{
			ASSERT_LV3(board[sq] == NO_PIECE);
			pc = packer.read_board_piece_from_stream();
		}
		else
		{
			pc = board[sq];
			board[sq] = NO_PIECE; 
		}

		if (pc == NO_PIECE)
			continue;

		put_piece(Piece(pc), sq);

	#if defined(USE_EVAL_LIST)
		PieceNumber piece_no =
			(pc == B_KING) ? PIECE_NUMBER_BKING : 
			(pc == W_KING) ? PIECE_NUMBER_WKING : 
			piece_no_count[raw_type_of(pc)]++; 

		evalList.put_piece(piece_no, sq, pc); 
	#endif


		if (stream.get_cursor() > 256)
			return Tools::Result(Tools::ResultCode::SomeError);
	}

	hand[BLACK] = hand[WHITE] = (Hand)0;

	int i = 0;
	Piece lastPc = NO_PIECE;

	while (stream.get_cursor() < 256)
	{
		auto pc = packer.read_hand_piece_from_stream();

		if (is_promoted(pc))
			continue;

		add_hand(hand[(int)color_of(pc)], type_of(pc));

	#if defined(USE_EVAL_LIST)
		if (lastPc != pc)
			i = 0;
		lastPc = pc;

		PieceType rpc = raw_type_of(pc);

		PieceNumber piece_no = piece_no_count[rpc]++;
		ASSERT_LV1(is_ok(piece_no));
		evalList.put_piece(piece_no, color_of(pc), rpc, i++);
	#endif
	}

	if (stream.get_cursor() != 256)
	{
		return Tools::Result(Tools::ResultCode::SomeError);
	}

	gamePly = gamePly_;

	update_bitboards();
	update_kingSquare();

	set_state();


	#if defined (LONG_EFFECT_LIBRARY)
	LongEffect::calc_effect(*this);
	#endif


#if defined(USE_PIECE_VALUE)
	st->materialValue = Eval::material(*this);
#endif

#if defined(CYFAMATE_NNUE_5GP)
	Cyfamate::gp_init_parts(*this, st);
#endif

#if defined(USE_CLASSIC_EVAL)
	Eval::compute_eval(*this);
#endif


	update_entering_point();




	return Tools::Result::Ok();
}

std::string Position::sfen_from_rawdata(Piece board[81], Hand hands[2], Color turn, int gamePly_)
{
	Position pos;

	memcpy(&pos.board, board, sizeof(Piece) * 81);
	memcpy(pos.hand, hands, sizeof(Hand) * 2);
	pos.sideToMove = turn;
	pos.gamePly = gamePly_;

	return pos.sfen();

}

void Position::sfen_pack(PackedSfen& sfen)
{
	SfenPacker sp;
	sp.data = (u8*)&sfen;
	sp.pack(*this);
}

std::string Position::sfen_unpack(const PackedSfen& sfen)
{
	SfenPacker sp;
	sp.data = (u8*)&sfen;
	return sp.unpack();
}

} 
