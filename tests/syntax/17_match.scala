// Syntakticky test: match so vsetkymi druhmi vzorov, straze, blok ako telo vetvy.
// Platny IFJ26 kod.
// Pravidla: 47-58
// Ocakavany vysledok: kod 0.
import ifj26.*
case class Point(x: Int, y: Int)
case class Nothing0()
def describe(value: Int | String | Point): String =
    value match
        case 0 => "zero"
        case -1 => "minus one"
        case 1.5 => "double"
        case -2.5 => "negative double"
        case "s" => "string"
        case true => "t"
        case false => "f"
        case () => "unit"
        case n: Int if n => "int"
        case text: String => text
        case Point(a, b) => a
        case Point(_, 1) => "literal subpattern"
        case Point(-1, "x") => "negative subpattern"
        case Nothing0() => "empty constructor"
        case p: Point =>
            val q = p
            q
        case _ => "default"
def inValue(x: Int) =
    val r = x match
        case _ => 1
    r
def inlineMatch(x: Int) = x match
    case 1 => 2
    case _ => 3
