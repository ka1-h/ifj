import ifj26.*

extension (value: Int)
    def twice: Int = value * 2

@main def start(): Unit =
    val result = twice()
    ()
