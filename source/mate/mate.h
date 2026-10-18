#ifndef MATE_H_INCLUDED
#define MATE_H_INCLUDED

#include "../types.h"
#if defined (USE_MATE_1PLY)

#include <vector>
#include <memory> 
#include <atomic> 

#include "../thread.h"

namespace YaneuraOu {

namespace Mate {

	void init();

	Move mate_1ply(const Position& pos);

	template <Color Us>
	Move mate_1ply_imp(const Position& pos);

	Move weak_mate_3ply(const Position& pos, int ply);

	enum class MateRepetitionState
	{
		Mate   , 
		Mated  , 
		Unknown, 
	};

	constexpr int MAX_REPETITION_PLY = 16;

	MateRepetitionState mate_repetition(const Position& pos, const int plies_from_root , const int repetition_ply, bool or_node);


#if defined(USE_MATE_SOLVER)|| defined(USE_MATE_DFPN)


	struct alignas(16) MateHashEntry
	{
		u64 board_key : 48;

		u64 root_color : 1;

		Color side_to_move() const { return (Color)(board_key & 1); }


		u64 is_mate   : 1; 

		u64 ply       : 11;

		u64 padding1  : 2;

		Move get_move() const { return (Move)(move16 + (move8 << 16)); }
		void set_move(Move move) { move16 = move.to_u16(); move8 = move.to_u32() >> 16; }

		Hand get_hand() const { return (Hand)hand; }

		void lock();

		void unlock();

		void save(Key board_key, Color root_color,  Hand hand, bool is_mate, u32 ply, Move move);


	private:
		u16 move16;
		u8  move8;
		u32 hand;

		std::atomic<bool> mutex;

	};

	class MateHashTable
	{
	public:

		MateHashEntry* first_entry(const Key board_key, Color side_to_move) const;

		void resize(size_t mbSize);

		void clear(ThreadPool& threads);

	private:
		MateHashEntry* table = nullptr;

		size_t entryCount = 0;
	};

#endif 

#if defined(USE_MATE_SOLVER)

	class MateSolver
	{
	public:
		Move mate_odd_ply(Position& pos, const int ply, bool gen_all);

		void set_max_game_ply(int max_game_ply) { this->max_game_ply = max_game_ply; }

	private:

		template <bool INCHECK, bool GEN_ALL>
		Move mate_odd_ply(Position& pos, const int ply);

		Move mated_even_ply(Position& pos, const int ply, bool gen_all);

		template <bool GEN_ALL>
		Move mated_even_ply(Position& pos, const int ply);

		template <bool INCHECK, bool GEN_ALL>
		Move mate_3ply(Position& pos);

	private:
		int root_game_ply;

		int max_game_ply = 0;
	};

#endif 
}

#if defined(USE_MATE_DFPN)

namespace Mate::Dfpn
{
	class MateDfpnSolverInterface
	{
	public:
		virtual void alloc(size_t size_mb) = 0;

		virtual void alloc_by_nodes_limit(size_t nodes_limit) = 0;

		virtual void set_hash_table(MateHashTable* hash_table) = 0;

		virtual Move mate_dfpn(const Position& pos , u64 nodes_limit)= 0;

		virtual void set_max_game_ply(int max_game_ply) = 0;

		virtual std::vector<Move> get_pv() const = 0;

		virtual std::vector<Move> get_current_pv() const = 0;

		virtual u64 get_nodes_searched() const = 0;

		virtual int get_mate_ply() const= 0;

		virtual void dfpn_stop(const bool stop) { this->stop = stop; };

		virtual bool is_out_of_memory() const= 0;

		virtual int hashfull() const = 0;

		virtual ~MateDfpnSolverInterface() {}

	protected:
		bool stop = false;
	};

	enum class DfpnSolverType
	{
		None,              

		Node32bit        , 
		Node16bitOrdering, 
		Node64bit        , 
		Node48bitOrdering, 

		Node32bitWithHash,
		Node16bitOrderingWithHash,
		Node64bitWithHash,
		Node48bitOrderingWithHash,


	};

	class MateDfpnSolver : public MateDfpnSolverInterface
	{
	public:
		MateDfpnSolver(DfpnSolverType t);

		void ChangeSolverType(DfpnSolverType t);

		virtual void alloc(size_t size_mb) { impl->alloc(size_mb); }

		virtual void alloc_by_nodes_limit(size_t nodes_limit) { impl->alloc_by_nodes_limit(nodes_limit); }

		virtual void set_hash_table(Mate::MateHashTable* table) { impl->set_hash_table(table); }

		virtual Move mate_dfpn(const Position& pos, u64 nodes_limit) { return impl->mate_dfpn(pos, nodes_limit); }

		virtual void set_max_game_ply(int max_game_ply) { impl->set_max_game_ply(max_game_ply); }

		virtual std::vector<Move> get_pv() const { return impl->get_pv(); }

		virtual std::vector<Move> get_current_pv() const { return impl->get_current_pv(); }

		virtual u64 get_nodes_searched() const { return impl->get_nodes_searched(); }

		virtual int get_mate_ply() const { return impl->get_mate_ply(); }

		virtual bool is_out_of_memory() const { return impl->is_out_of_memory(); }

		virtual int hashfull() const { return impl->hashfull(); }

	private:
		std::unique_ptr<MateDfpnSolverInterface> impl;
	};

} 
#endif 

} 
#endif 

#endif 
