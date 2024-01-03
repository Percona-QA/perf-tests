#!/usr/bin/python

import sys

if len(sys.argv) < 2:
    print(f"Usage: python {sys.argv[0]} <data_file>")
    print(f"or     python {sys.argv[0]} <data_file> <data_file2>")
    sys.exit(1)

data_file = sys.argv[1]

def parse_line(line):
    # Remove leading and trailing spaces and the trailing comma
    line = line.strip().strip(', []')
    if not line or line[0] == '#':
        return ""
    # Split the line by comma and convert numeric values to floats
    values = [val.strip() for val in line.split(',')]
    if values[0] == 'Run':
        return ""
    values = [values[0]] + [float(val.strip()) for val in values[1:]]
    return values

def parse_table(table):
    if (len(table) == 0):
        return

    for i in range(len(table) - 1):
        line1 = table[i]
        line2 = table[i + 1]

        if line1 == "" or line2 == "":
            if line1 == "":
                print()
            continue

        print(f"{line1[0]} vs {line2[0]}, ", end='')
        for j in range(1, len(line1)):
            res=0
            if line2[j] != 0:
                res=line1[j]/line2[j]*100
            print(f"{res:.2f}, ", end='')
        print()

def compare_tables(table, table2):
    if (len(table) == 0) or (len(table2) == 0):
        return

    for i in range(len(table)):
        line1 = table[i]
        line2 = table2[i]

        if line1 == "" or line2 == "":
            if line1 == "":
                print()
            continue

        print(f"{line1[0]} vs {line2[0]}, ", end='')
        for j in range(1, len(line1)):
            res=0
            if line2[j] != 0:
                res=line1[j]/line2[j]*100
            print(f"{res:.2f}, ", end='')
        print()

# Read data from the file
table = []
if len(sys.argv) == 2:
    with open(data_file, "r") as file:
        for line in file:
            values=parse_line(line)
            if values != "":
                table.append(values)
            else:
                parse_table(table)
                table = []
        parse_table(table)
else:
    data_file2 = sys.argv[2]
    table2 = []
    with open(data_file, "r") as file:
        for line in file:
            values=parse_line(line)
            table.append(values)
    with open(data_file2, "r") as file:
        for line in file:
            values=parse_line(line)
            table2.append(values)
    compare_tables(table, table2)
