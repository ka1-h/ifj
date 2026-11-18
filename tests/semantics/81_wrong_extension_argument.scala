import ifj26.*

extension (value: Int)
    def plus(other: Int): Int = value + other

@main def start(): Unit =
    val number = 3
    val result = number.plus("text")
    ()
