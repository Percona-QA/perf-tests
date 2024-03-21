#!/usr/bin/python

# Convert results of workloads to a single line for each run:
# node4-8036F1_time300_stable-innodb_PS8036-28-OLTP_DELETE-16x10M-96G, 33779.19, 59486.94, 78095.08, 78067.37,
# node4-8036F2_time300_stable-innodb_PS8036-28-OLTP_DELETE-16x10M-96G, 33852.49, 59264.33, 78287.22, 78730.06,
# node4-8036F3_time300_stable-innodb_PS8036-28-OLTP_DELETE-16x10M-96G, 33883.43, 59201.15, 77993.93, 77515.09,
# node4-8036F1_time300_stable-innodb_PS8036-28-OLTP_DELETE_INSERTS-16x10M-96G, 60014.04, 113292.51, 157095.12, 168561.33,
# node4-8036F2_time300_stable-innodb_PS8036-28-OLTP_DELETE_INSERTS-16x10M-96G, 60425.63, 113592.04, 156976.99, 169328.69,
# node4-8036F3_time300_stable-innodb_PS8036-28-OLTP_DELETE_INSERTS-16x10M-96G, 60183.68, 112910.28, 157153.14, 169096.80,
# node4-8036F1_time300_stable-innodb_PS8036-28-OLTP_INSERT-16x10M-96G, 36452.95, 57944.37, 58287.64, 54270.75,
# node4-8036F2_time300_stable-innodb_PS8036-28-OLTP_INSERT-16x10M-96G, 36402.03, 58155.07, 57791.38, 53718.36,
# node4-8036F3_time300_stable-innodb_PS8036-28-OLTP_INSERT-16x10M-96G, 36409.83, 57753.01, 57942.51, 54301.44,
# node4-8036F1_time300_stable-innodb_PS8036-28-OLTP_POINT_SELECT-16x10M-96G, 64226.85, 125336.44, 193792.90, 299183.56,
# node4-8036F2_time300_stable-innodb_PS8036-28-OLTP_POINT_SELECT-16x10M-96G, 63901.85, 125200.96, 193942.26, 298927.44,
# node4-8036F3_time300_stable-innodb_PS8036-28-OLTP_POINT_SELECT-16x10M-96G, 63699.35, 124738.39, 193042.22, 297968.05,
# node4-8036F1_time300_stable-innodb_PS8036-28-OLTP_UPDATE_INDEX-16x10M-96G, 23701.76, 48147.95, 59307.68, 50416.28,
# node4-8036F2_time300_stable-innodb_PS8036-28-OLTP_UPDATE_INDEX-16x10M-96G, 23790.72, 48144.49, 59522.50, 51357.19,
# node4-8036F3_time300_stable-innodb_PS8036-28-OLTP_UPDATE_INDEX-16x10M-96G, 22853.50, 47925.55, 59138.97, 50306.09,
# node4-8036F1_time300_stable-innodb_PS8036-28-OLTP_UPDATE_NON_INDEX-16x10M-96G, 31331.57, 51209.65, 57190.46, 50978.49,
# node4-8036F2_time300_stable-innodb_PS8036-28-OLTP_UPDATE_NON_INDEX-16x10M-96G, 31593.01, 51394.82, 57011.62, 50512.44,
# node4-8036F3_time300_stable-innodb_PS8036-28-OLTP_UPDATE_NON_INDEX-16x10M-96G, 31504.14, 51176.82, 57069.33, 50969.81,
# to:
#                                                        , OLTP_DELETE,                            OLTP_DELETE_INSERTS,                       OLTP_INSERT,                            OLTP_POINT_SELECT,                        OLTP_UPDATE_INDEX,                      OLTP_UPDATE_NON_INDEX             
# node4-8036F1_time300_stable-innodb_PS8036-28-16x10M-96G, 33779.19, 59486.94, 78095.08, 78067.37, 60014.04, 113292.51, 157095.12, 168561.33, 36452.95, 57944.37, 58287.64, 54270.75, 64226.85, 125336.44, 193792.9, 299183.56, 23701.76, 48147.95, 59307.68, 50416.28, 31331.57, 51209.65, 57190.46, 50978.49
# node4-8036F2_time300_stable-innodb_PS8036-28-16x10M-96G, 33852.49, 59264.33, 78287.22, 78730.06, 60425.63, 113592.04, 156976.99, 169328.69, 36402.03, 58155.07, 57791.38, 53718.36, 63901.85, 125200.96, 193942.26, 298927.44, 23790.72, 48144.49, 59522.5, 51357.19, 31593.01, 51394.82, 57011.62, 50512.44
# node4-8036F3_time300_stable-innodb_PS8036-28-16x10M-96G, 33883.43, 59201.15, 77993.93, 77515.09, 60183.68, 112910.28, 157153.14, 169096.8,  36409.83, 57753.01, 57942.51, 54301.44, 63699.35, 124738.39, 193042.22, 297968.05, 22853.5, 47925.55, 59138.97, 50306.09, 31504.14, 51176.82, 57069.33, 50969.81

import sys

if len(sys.argv) < 3:
    print(f"Usage: python {sys.argv[0]} <data_file> <nth_line>")
    sys.exit(1)

data_file = sys.argv[1]
nth_line = int(sys.argv[2])

def parse_line(line):
    # Remove leading and trailing spaces and the trailing comma
    values = line.strip().strip(', []')
    if not values:
        return False
    # Split the line by comma and convert numeric values to floats
    values = [val.strip() for val in values.split(',')]
    values = [values[0]] + [float(val.strip()) for val in values[1:]]
    return values

# Read data from the file
data = []
with open(data_file, "r") as file:
    for line in file:
        values=parse_line(line)
        if values:
            #print(values)
            data.append(values)

# Iterate through the data and compare each line with the line n+nth_line
for i in range(nth_line):
    output = []
    title=data[i][0].replace("OLTP_DELETE-", "")
    output.append(title)
    #print(title);
    for j in range(i, len(data), nth_line):
        #print(f"{j:.2f}, ", end='')
        numbers = data[j][1:]
        line_str = ', '.join(map(str, numbers))
        output.append(line_str)
    print(', '.join(map(str, output)))
