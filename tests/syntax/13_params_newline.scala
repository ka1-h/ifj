// Syntakticky test: koniec riadku v zatvorkach a koncova ciarka pred koncom riadku.
// Platny IFJ26 kod (spec 1.2: LF v zatvorkach je biely znak; koncova ciarka je povolena,
// ak je medzi nou a zatvorkou koniec riadku).
// Pravidla: 7, 8, 9, 10, 11, 19
// Ocakavany vysledok: kod 0.
import ifj26.*
case class Big(
    a: Int,
    b: String,
)
def f(
    x: Int,
    y: Int
): Int = x
def g(x: Int,
      y: Int,
): Int =
    y
