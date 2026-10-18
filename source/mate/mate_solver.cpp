#include "mate.h"
#if defined(USE_MATE_SOLVER)
#include "../position.h"
#include "mate_move_picker.h"



namespace YaneuraOu {
namespace Mate {

	template <bool INCHECK , bool GEN_ALL>
	FORCE_INLINE Move MateSolver::mate_3ply(Position& pos)
	{

		StateInfo si;
		StateInfo si2;

		for (const auto& m : MovePicker<true, INCHECK , GEN_ALL , false >(pos))
		{
			pos.do_move(m, si, true);


			bool found_mate = false;

			auto rep = mate_repetition(pos, pos.game_ply() - root_game_ply , MAX_REPETITION_PLY , false);

			switch(rep)
			{
			case MateRepetitionState::Mated:
				found_mate = true;
				break;

			case MateRepetitionState::Mate:
				break;

			case MateRepetitionState::Unknown:

				MovePicker<false, false, GEN_ALL, false > move_picker2(pos);

				if (move_picker2.size() == 0) {
					found_mate = true;
				}
				else {
					for (const auto& m2 : move_picker2)
					{
						if (pos.gives_check(m2))
							goto NEXT_CHECK;

						pos.do_move(m2, si2, false);


						if (!Mate::mate_1ply(pos)) {
							pos.undo_move(m2);
							goto NEXT_CHECK;
						}

						pos.undo_move(m2);
					}
					found_mate = true;
				}
				break;

			}
			NEXT_CHECK:;
			pos.undo_move(m);

			if (found_mate)
				return m;
		}
		return Move::none();
	}

	template <bool INCHECK , bool GEN_ALL>
	Move MateSolver::mate_odd_ply(Position& pos, const int ply)
	{
		if (max_game_ply && max_game_ply < pos.game_ply())
			return Move::none();

		if (ply == 3)
			return mate_3ply<INCHECK,GEN_ALL>(pos);
		else if (ply == 1)
			return !pos.in_check() ? mate_1ply(pos) : Move::none();


		for (const auto& ml : MovePicker<true, INCHECK , GEN_ALL , false >(pos)) {

			auto m = Move(ml);

			StateInfo state;
			pos.do_move(m, state, true);


			bool found_mate = false;

			auto rep = mate_repetition(pos, pos.game_ply() - root_game_ply , MAX_REPETITION_PLY , false);

			switch (rep)
			{
			case MateRepetitionState::Mated:
				found_mate = true;
				break;

			case MateRepetitionState::Mate:
				break;

			case MateRepetitionState::Unknown:
				found_mate = mated_even_ply<GEN_ALL>(pos, ply - 1) == Move::none() ;
				break;

			default: UNREACHABLE;
			}

			pos.undo_move(m);

			if (found_mate)
				return m;
		}

		return Move::none();
	}
		
	template <bool GEN_ALL>
	Move MateSolver::mated_even_ply(Position& pos, const int ply)
	{
		MovePicker<false, false, GEN_ALL, false > picker(pos);

		if (max_game_ply && max_game_ply < pos.game_ply() && picker.size())
			return Move::win(); 


		for (const auto& ml : picker)
		{
			auto m = Move(ml);

			const bool givesCheck = pos.gives_check(m);

			StateInfo state;
			pos.do_move(m, state, givesCheck);


			bool found_escape = false;

			auto rep = mate_repetition(pos, pos.game_ply() - root_game_ply , MAX_REPETITION_PLY , true);

			switch (rep)
			{
			case MateRepetitionState::Mated:
				found_escape = true;
				break;

			case MateRepetitionState::Mate:
				break;

			case MateRepetitionState::Unknown:
				if (ply == 4)
					found_escape = !(givesCheck ? mate_3ply<true, GEN_ALL>(pos) : mate_3ply<false, GEN_ALL>(pos));
				else
					found_escape = !(givesCheck ? mate_odd_ply<true , GEN_ALL>(pos, ply - 1) : mate_odd_ply<false , GEN_ALL>(pos, ply - 1));
				break;

			default: UNREACHABLE;
			}

			pos.undo_move(m);

			if (found_escape)
				return m; 
		}

		return Move::none();
	}

	Move MateSolver::mated_even_ply(Position& pos, const int ply, bool gen_all)
	{
		return gen_all ? mated_even_ply<true>(pos, ply) : mated_even_ply<false>(pos, ply);
	}


	Move MateSolver::mate_odd_ply(Position& pos, const int depth , bool GEN_ALL)
	{
		root_game_ply = pos.game_ply();

		return GEN_ALL ? (pos.in_check() ? mate_odd_ply<true,true >(pos, depth) : mate_odd_ply<false,true >(pos, depth))
		               : (pos.in_check() ? mate_odd_ply<true,false>(pos, depth) : mate_odd_ply<false,false>(pos, depth));
	}


} 
} 

#endif 
