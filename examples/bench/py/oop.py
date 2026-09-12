class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y
    def add(self, other):
        self.x += other.x
        self.y += other.y

total = 0
for i in range(5000000):
    a = Point(i, i + 1)
    b = Point(1, 1)
    a.add(b)
    total += a.x + a.y
print(total)