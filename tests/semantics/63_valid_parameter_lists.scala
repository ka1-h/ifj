import ifj26.*

def answer: Int = 42
def add(a: Int)(b: Int): Int = a + b
def offset()(value: Int): Int = value + 1

@main def start(): Unit =
    val x = answer
    val y = add(x)(2)
    val z = offset()(y)
    print(z)
    ()
