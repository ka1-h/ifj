import ifj26.*

case class Point(x: Int, y: Double)

@main def start(): Unit =
    val point: Point = Point(1, 2.5)
    val x: Int = point.x
    val y: Double = point.y
    ()
