// Syntakticky test: vnoreny vzor konstruktora (spec 5.5: vnorene vzory nie su podporovane).
// Neplatny IFJ26 kod.
// Ocakavany vysledok: kod 2.
import ifj26.*
def f(x: Int) =
    x match
        case P(Q(a)) => 1
        case _ => 2
