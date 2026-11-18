import ifj26.*

def twice(): Int = 10

@main def start(): Unit =
    val number = 3
    val a: Int = number.twice
    val b: Int = number.plus(2)(1)
    val c: String = number.label()
    val text = "a"
    val d: String = text.twice
    val e: Int = twice()
    ()

extension (value: Int)
    def twice = value * 2
    def plus(other: Int)(extra: Int): Int = value + other + extra
    def label(): String = toStr(value)

extension (value: String)
    def twice: String = value + value
