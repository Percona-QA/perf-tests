# Compare results in the following format:
#'404_mysql-innodb_oltp_update_index-16x10M-32G', 790.25, 1299.29, 12216.47, 18231.34, 21488.34, 24436.32, 24334.07, 24111.19
#'405_mysql-innodb_oltp_update_index-16x10M-32G', 784.03, 1632.28, 12216.99, 18234.55, 21589.48, 24523.99, 24448.70, 24290.00
#'406_mysql-innodb_oltp_update_index-16x10M-32G', 782.61, 2227.35, 11688.03, 17833.33, 21035.12, 24073.96, 23991.78, 23903.01
#
#'410_mysql-innodb_oltp_update_non_index-16x10M-32G', 1962.34, 6611.11, 13843.98, 22881.56, 24637.47, 26806.74, 27571.01, 27471.77
#'411_mysql-innodb_oltp_update_non_index-16x10M-32G', 2033.17, 6742.66, 14136.28, 23142.92, 25105.09, 26997.37, 27551.11, 27572.84
#'412_mysql-innodb_oltp_update_non_index-16x10M-32G', 2010.27, 6514.91, 13918.65, 22981.93, 24829.87, 27135.74, 27830.83, 27764.42

import sys

if len(sys.argv) < 2:
    print(f"Usage: python {sys.argv[0]} <data_file>")
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

# Read data from the file
data = []
with open(data_file, "r") as file:
    for line in file:
        values=parse_line(line)
        data.append(values)

# Iterate through the data and compare each line with the line n-1
for i in range(len(data) - 1):
    line1 = data[i]
    line2 = data[i + 1]

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
