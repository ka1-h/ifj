// Syntakticky test: extension s viacerymi metodami, @main na jednom riadku s def.
// Platny IFJ26 kod.
// Pravidla: 4.1, 4.5, 5.2, 19, 20, 21
// Ocakavany vysledok: kod 0.
import ifj26.*
extension (value: Int)
    // bezparametricka metoda
    def twice: Int = value
    def foo(): String = "foo"
    def plus(other: Int): Int =
        val r = other
        r
extension (s: String)
    def same: String = s
@main def main(): Unit =
    return
