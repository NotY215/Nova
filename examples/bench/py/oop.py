class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y
    def mag_sq(self):
        return self.x * self.x + self.y * self.y

total = 0
i = 0
while i < 5000000:
    p = Point(i, i + 1)
    total += p.mag_sq()
    i += 1
print(total)