% Critic predicates for the autodidactic motor (Horn-clause shaped).
% Loaded together with the growing archive.pl theory.
%
% Conventions:
%   obs(Seq, N, V)           — ground observation
%   rec(Seq, Coeffs)         — verified linear recurrence (archived)
%   rejected(Name, Why)      — finite-fail archive of dead ends
%   verified(fact(World, Family, Name, Formula))
%   lemma(Name, Kind, Formula)
%   holds_bit(ExId, BitList, Out) — logic world examples
%   bit_fn(Name, Kind, Spec) — learned boolean hypothesis

:- dynamic obs/3.
:- dynamic rec/2.
:- dynamic rejected/2.
:- dynamic verified/1.
:- dynamic lemma/3.
:- dynamic holds_bit/3.
:- dynamic bit_fn/3.
:- dynamic companion/2.
:- dynamic true_mod/3.   % true_mod(Seq, M, Period) Pisano-style

% --- Sequence critic: recurrence holds on all obs with N >= order ---

pred_from_rec(Seq, N, Coeffs, Pred) :-
    pred_from_rec_(Seq, N, Coeffs, 1, 0, Pred).

pred_from_rec_(_, _, [], _, Acc, Acc) :- !.
pred_from_rec_(Seq, N, [C|Cs], K, Acc, Pred) :-
    N1 is N - K,
    obs(Seq, N1, V),
    Acc1 is Acc + C * V,
    K1 is K + 1,
    pred_from_rec_(Seq, N, Cs, K1, Acc1, Pred).

holds_rec(Seq, Coeffs) :-
    length(Coeffs, Order),
    Order > 0,
    forall(
        (obs(Seq, N, V), N >= Order),
        (pred_from_rec(Seq, N, Coeffs, Pred), Pred =:= V)
    ).

% Next-term prediction (for transfer accuracy metrics)
next_term(Seq, N, Coeffs, Pred) :-
    pred_from_rec(Seq, N, Coeffs, Pred).

% Transfer: reuse a recurrence clause from a companion sequence
transfer_prior(Target, Coeffs, Source) :-
    rec(Source, Coeffs),
    Source \= Target,
    companion(Source, Target).

% --- Logic critic: evaluate bit functions on examples ---
% Spec forms:
%   parity      — XOR of all bits
%   and_all     — conjunction
%   xor2        — first two bits XOR (pad false)
%   const(B)    — constant

eval_bits(parity, Bits, Out) :-
    foldl(xor_bit, Bits, 0, Out).
eval_bits(and_all, Bits, Out) :-
    ( member(0, Bits) -> Out = 0 ; Out = 1 ).
eval_bits(xor2, Bits, Out) :-
    ( Bits = [A,B|_] -> Out is A xor B
    ; Bits = [A] -> Out = A
    ; Out = 0 ).
eval_bits(const(B), _, B).

xor_bit(A, Acc, R) :- R is A xor Acc.

holds_bit_fn(Kind) :-
    forall(
        holds_bit(_Id, Bits, Out),
        (eval_bits(Kind, Bits, Pred), Pred =:= Out)
    ).

% --- Modular / Pisano helper ---
obs_mod(Seq, M, N, R) :-
    obs(Seq, N, V),
    R is V mod M.

holds_period(Seq, M, Period) :-
    Period > 0,
    forall(
        (obs(Seq, N, _), N >= Period),
        (obs_mod(Seq, M, N, R),
         N0 is N - Period,
         obs_mod(Seq, M, N0, R0),
         R =:= R0)
    ).

% --- Query helpers used by the Python bridge ---
% succeed/fail is the critic verdict.

check_rec(Seq, Coeffs) :- holds_rec(Seq, Coeffs).
check_bit_fn(Kind) :- holds_bit_fn(Kind).
check_period(Seq, M, Period) :- holds_period(Seq, M, Period).

% List archived verified names
list_verified(Names) :-
    findall(N, verified(fact(_, _, N, _)), Names).

list_recs(Recs) :-
    findall(rec(S, C), rec(S, C), Recs).

list_lemmas(Ls) :-
    findall(lemma(N, K, F), lemma(N, K, F), Ls).
