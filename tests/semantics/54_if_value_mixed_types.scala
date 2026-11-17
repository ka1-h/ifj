import ifj26.*
@main def start(): Unit =
    val x: Double = if true then
        val y = 2
        y
    else
        3.5
    print(x.toInt)
