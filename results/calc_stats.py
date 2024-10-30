import csv
import numpy as np
import sys

def main(csv_file):
    # Open the CSV file
    with open(csv_file, newline='') as f:
        reader = csv.reader(f)
        # Skip the header row
        # next(reader)
        # Read the remaining rows
        data = []
        for row in reader:
            # Filter out empty columns at the end of the row
            filtered_row = [float(val) for val in row[1:] if val.strip()]
            data.append(filtered_row)

    # Convert data to numpy array
    data = np.array(data)
    print(data)

    # Calculate median, standard deviation, average, and difference between max and min for each column
    avg = np.mean(data, axis=0)
    median = np.median(data, axis=0)
    std_dev = np.std(data, axis=0)
    std_dev_percent = (std_dev / avg) * 100
    max_val = np.max(data, axis=0)
    min_val = np.min(data, axis=0)
    diff_max_min = max_val - min_val
    diff_max_min_percent = (diff_max_min / avg) * 100

    # Print the results
    for i, (med, std, av, diff) in enumerate(zip(median, std_dev, avg, diff_max_min)):
        print(f"Column {i+1}: Median={med:.2f} StdDeviation={std:.2f} ({std*100/av:.2f}%) Average={av:.2f} MaxMinDiff={diff:.2f} ({diff*100/av:.2f}%)")

    # Print the results in CSV-like format
    print("Average," + ",".join(f"{val:.2f}" for val in avg))
    print("Median," + ",".join(f"{val:.2f}" for val in median))
    print("Standard Deviation," + ",".join(f"{val:.2f}" for val in std_dev))
    print("Standard Deviation %," + ",".join(f"{val:.2f}" for val in std_dev_percent))
    print("Max-Min Difference," + ",".join(f"{val:.2f}" for val in diff_max_min))
    print("Max-Min Difference %," + ",".join(f"{val:.2f}" for val in diff_max_min_percent))

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script.py input.csv")
        sys.exit(1)
    csv_file = sys.argv[1]
    main(csv_file)
