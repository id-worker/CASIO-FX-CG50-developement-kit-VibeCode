# PyCEdit: Python syntax highlighting test
# Open this .py file and scroll through it.

"""
This is a multiline string.
A # symbol here is text, not a comment.
Numbers such as 42 are part of the string too.
"""

import math

TITLE = "Sensor report"
READINGS = [18, 23, 12, 27, 20, 14, 25, 19]
LOW = 15
HIGH = 24


def classify(value):
    if value < LOW:
        return "LOW"
    elif value > HIGH:
        return "HIGH"
    else:
        return "OK"


def average(values):
    total = 0
    for value in values:
        total = total + value

    if len(values) == 0:
        return 0
    return total / len(values)


def count_ok(values):
    count = 0
    for value in values:
        if classify(value) == "OK":
            count = count + 1
    return count


def show_readings(values):
    print(TITLE)
    print("-" * 24)

    for index in range(len(values)):
        value = values[index]
        label = classify(value)
        print("Reading", index + 1, ":", value, label)

    print("Average:", average(values))
    print("In range:", count_ok(values))


def show_strings():
    # Symbols inside quotes should remain string text.
    print("A # here is not a comment")
    print("C-like // symbols are just text")
    print("Quoted word: \"hello\"")
    print('A single-quoted string')
    print(r"Path: C:\TEMP\file.py")


def main():
    show_readings(READINGS)
    show_strings()
    print("Square root:", math.sqrt(81))
    print("Hex number:", 0x2A)
    print("Done!")


main()
