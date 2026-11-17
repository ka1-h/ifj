import ifj26.*
@main def start(): Unit =
    print(1 + twice(toInt(2.5) + 1))
    ()

def twice(x: Int): Int = x * 2
