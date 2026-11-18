import ifj26.*

case class Point(x: Int, y: Int)
type Value = Int | String | Point

def describe(value: Value): String =
    value match
        case 0 => "zero"
        case number: Int if number > 0 => toStr(number)
        case text: String => text
        case Point(x, y) => toStr(x + y)
        case _ => "other"

@main def start(): Unit =
    val text: String = describe(Point(1, 2))
    ()
