#!/bin/python

import os
import glob
import re
import sys
import math
import argparse

def parse_file(filepath):
    stats = {
        'thds': [],
        'qps': [],
        'qps_r': [],
        'qps_w': [],
        'qps_o': [],
        'lat': []
    }
    
    # Regex to match the line
    # [ 10s ] thds: 8 tps: 1029.40 qps: 11327.01 (r/w/o: 9267.41/0.00/2059.60) lat (ms,99%): 9.56 err/s: 0.00 reconn/s: 0.00
    pattern = re.compile(r"\[\s+\d+s\s+\]\s+thds:\s+(?P<thds>\d+)\s+tps:\s+(?P<tps>[\d\.]+)\s+qps:\s+(?P<qps>[\d\.]+)\s+\(r/w/o:\s+(?P<qps_r>[\d\.]+)/(?P<qps_w>[\d\.]+)/(?P<qps_o>[\d\.]+)\)\s+lat\s+\(ms,99%\):\s+(?P<lat>[\d\.]+)")
    
    try:
        with open(filepath, 'r') as f:
            for line in f:
                match = pattern.search(line)
                if match:
                    data = match.groupdict()
                    stats['thds'].append(float(data['thds']))
                    stats['qps'].append(float(data['qps']))
                    stats['qps_r'].append(float(data['qps_r']))
                    stats['qps_w'].append(float(data['qps_w']))
                    stats['qps_o'].append(float(data['qps_o']))
                    stats['lat'].append(float(data['lat']))
    except Exception as e:
        print(f"Error reading {filepath}: {e}", file=sys.stderr)
        return None

    return stats

def calculate_stats(values):
    if not values:
        return 0.0, 0.0
    
    mean = sum(values) / len(values)
    if len(values) > 1:
        variance = sum([((x - mean) ** 2) for x in values]) / (len(values) - 1) # Sample variance
        std_dev = math.sqrt(variance)
    else:
        std_dev = 0.0
    return mean, std_dev

def filter_stats(stats):
    # Use 'qps' as the reference metric for filtering
    qps_values = stats['qps']
    n = len(qps_values)
    
    if n < 2:
        return stats, 0
    
    # Always use the logic that finds the suffix with length >= n/2 that has the minimum Std Dev %
    min_std_pct = float('inf')
    best_cutoff = 0 # Default to keeping everything if something goes wrong
    
    # We can start cutoff from 0 up to n - ceil(n/2)
    # kept_len = n - cutoff. kept_len >= n/2 => cutoff <= n - n/2
    max_cutoff = int(n - math.ceil(n/2))
    
    for i in range(0, max_cutoff + 1):
        suffix = qps_values[i:]
        mean, std = calculate_stats(suffix)
        if mean == 0:
            pct = 0
        else:
            pct = (std / mean) * 100
        
        if pct < min_std_pct:
            min_std_pct = pct
            best_cutoff = i
    
    cutoff_index = best_cutoff

    skipped = cutoff_index
    
    # Apply skip if needed
    if skipped > 0:
         new_stats = {}
         for key in stats:
             new_stats[key] = stats[key][skipped:]
         return new_stats, skipped
         
    return stats, 0

def main():
    parser = argparse.ArgumentParser(description="Parse sysbench logs.")
    parser.add_argument("directories", nargs='+', default=["."], help="Directories containing log files")
    args = parser.parse_args()

    directories = args.directories
    
    # Data structure for summary table
    # summary_data[directory][workload][threads] = qps
    summary_data = {}
    dir_prefixes = {} # Store common prefix per directory
    all_threads = set()
    
    # List to store high variance configs
    high_variance_configs = []

    print(f"{'File Name':<55} | {'Metric':<10} | {'Average':<15} | {'Std Dev %':<15}")
    print("-" * 104)

    for directory in directories:
        if not os.path.isdir(directory):
            print(f"Error: Directory '{directory}' not found.")
            continue

        # Use directory name as label
        dir_label = os.path.basename(os.path.normpath(directory)).rstrip('_')
        if dir_label not in summary_data:
            summary_data[dir_label] = {}

        # Find files matching *[number].txt
        files = glob.glob(os.path.join(directory, "*.txt"))
        target_files = []
        for f in files:
            # Match files that end with a number and .txt, e.g. "TRX-8.txt"
            if re.search(r'\d+\.txt$', f):
                target_files.append(f)
        
        target_files.sort()

        if not target_files:
            print(f"No matching files found in {directory}")
            continue
            
        # Print dir label as comment in console output
        print(f"# Directory: {dir_label}")
        
        # Calculate common prefix for this directory
        file_prefixes = []

        for filepath in target_files:
            filename = os.path.basename(filepath)
            stats = parse_file(filepath)
            stats_all = stats # Save unfiltered stats
            
            # Extract prefix
            prefix_match = re.search(r'^(.*)_([A-Z_]+)-(\d+)\.txt$', filename)
            if prefix_match:
                file_prefixes.append(prefix_match.group(1))
            
            row_count = 0
            skipped_count = 0
            
            if stats and stats['thds']:
                stats, skipped_count = filter_stats(stats)
                row_count = len(stats['thds'])
                
                # Extract workload and threads for summary table
                match = re.search(r'_([A-Z_]+)-(\d+)\.txt$', filename)
                if match:
                    workload = match.group(1)
                    threads = int(match.group(2))
                    
                    # Calculate average QPS
                    avg_qps, _ = calculate_stats(stats['qps'])
                    
                    if workload not in summary_data[dir_label]:
                        summary_data[dir_label][workload] = {}
                    summary_data[dir_label][workload][threads] = avg_qps
                    all_threads.add(threads)

            # Split filename into chunks of 55 chars
            chunk_size = 55
            file_chunks = [filename[i:i+chunk_size] for i in range(0, len(filename), chunk_size)]
            
            # Add Rows info as the last chunk
            rows_info = f"(Rows: {row_count}"
            if skipped_count > 0:
                rows_info += f", Skipped: {skipped_count}"
            rows_info += ")"
            file_chunks.append(rows_info)

            if not stats or not stats['thds']:
                for chunk in file_chunks:
                    print(f"{chunk:<55} | {'':<10} | {'':<15} | {'':<15}")
                print("-" * 104)
                continue

            metrics = ['thds', 'qps', 'qps_all', 'qps_r', 'qps_w', 'qps_o', 'lat']
            
            max_rows = max(len(metrics), len(file_chunks))
            
            for i in range(max_rows):
                # File Name Column
                if i < len(file_chunks):
                    f_col = file_chunks[i]
                else:
                    f_col = ""
                
                # Metric Columns
                if i < len(metrics):
                    metric = metrics[i]
                    
                    if metric == 'qps_all':
                        current_values = stats_all['qps']
                        m_label = 'qps (all)'
                    else:
                        current_values = stats[metric]
                        m_label = metric
                        
                    avg, std = calculate_stats(current_values)
                    
                    # Calculate percentage
                    if avg != 0:
                        pct = (std / avg) * 100
                    else:
                        pct = 0.0
                    
                    # Check for high variance (Std Dev % > 10%)
                    # We check 'qps' metric specifically as it's the primary one, 
                    # but the requirement says "print all configs... where Std Dev % > 10%"
                    # Assuming we check 'qps' metric for the config.
                    if metric == 'qps' and pct > 10.0:
                        high_variance_configs.append({
                            'name': filename,
                            'std_dev_pct': pct
                        })

                    # Format avg
                    if avg > 9999999:
                        avg_str = f"{avg:.2e}"
                    else:
                        avg_str = f"{avg:.2f}"
                    
                    # Format pct
                    if pct > 9999999:
                        pct_str = f"{pct:.2e}%"
                    else:
                        pct_str = f"{pct:.2f}%"

                    print(f"{f_col:<55} | {m_label:<10} | {avg_str:<15} | {pct_str:<15}")
                else:
                    # Just print file chunk if metrics are done
                     print(f"{f_col:<55} | {'':<10} | {'':<15} | {'':<15}")

            print("-" * 104)
            
        # Determine common prefix for directory
        if file_prefixes:
            common = os.path.commonprefix(file_prefixes).rstrip('_')
            dir_prefixes[dir_label] = common
        else:
            dir_prefixes[dir_label] = ""

    # Print High Variance Configs
    if high_variance_configs:
        print("\nConfigurations with Std Dev % > 10%:")
        print(f"{'File Name':<55} | {'Std Dev %':<15}")
        print("-" * 73)
        for config in high_variance_configs:
             print(f"{config['name']:<55} | {config['std_dev_pct']:.2f}%")
        print("-" * 73)
    else:
        print("\nNo configurations with Std Dev % > 10% found.")


    # Generate HTML
    html_filename = "sysbench_summary.html"
    sorted_threads = sorted(list(all_threads))
    
    # Classify directories into Left (default) and Right (rocksdb)
    left_dirs = []
    right_dirs = []
    
    processed_labels = set()
    for directory in directories:
        if not os.path.isdir(directory): continue
        dir_label = os.path.basename(os.path.normpath(directory))
        if dir_label not in summary_data: continue
        
        # Skip directories with no data
        if not summary_data[dir_label]: continue
        
        if dir_label in processed_labels: continue
        
        processed_labels.add(dir_label)
        prefix = dir_prefixes.get(dir_label, "")
        
        # Check if prefix contains "rocksdb"
        if "rocksdb" in prefix.lower():
            right_dirs.append(dir_label)
        else:
            left_dirs.append(dir_label)
    
    with open(html_filename, 'w') as f:
        f.write("<html>\n<head>\n<style>\n")
        f.write("  body { font-family: Arial, sans-serif; }\n")
        f.write("  table { border-collapse: collapse; width: 100%; }\n")
        f.write("  th, td { border: 1px solid #000000; padding: 8px; text-align: left; }\n")
        f.write("  .header { background-color: #efefef; font-weight: bold; }\n")
        f.write("  .comment { background-color: #d9ead3; font-style: italic; }\n")
        f.write("  .spacer { border: none; width: 20px; background-color: white; }\n")
        f.write("</style>\n</head>\n<body>\n")
        
        max_len = max(len(left_dirs), len(right_dirs))
        
        if max_len > 0:
            f.write("<table>\n")
            
            for i in range(max_len):
                l_label = left_dirs[i] if i < len(left_dirs) else None
                r_label = right_dirs[i] if i < len(right_dirs) else None
                
                n_cols = 1 + len(sorted_threads) # Workload + threads
                
                # Row 1: Directory Labels
                f.write("  <tr>\n")
                if l_label:
                    f.write(f"    <td class='comment' colspan='{n_cols}'>{l_label}</td>\n")
                else:
                    f.write(f"    <td colspan='{n_cols}' style='border:none;'></td>\n")
                
                f.write("    <td class='spacer'></td>\n")
                
                if r_label:
                    f.write(f"    <td class='comment' colspan='{n_cols}'>{r_label}</td>\n")
                else:
                    f.write(f"    <td colspan='{n_cols}' style='border:none;'></td>\n")
                f.write("  </tr>\n")
                
                # Row 2: Prefix Labels
                f.write("  <tr>\n")
                if l_label:
                    p = dir_prefixes.get(l_label, "")
                    f.write(f"    <td class='comment' colspan='{n_cols}'>{p}</td>\n")
                else:
                    f.write(f"    <td colspan='{n_cols}' style='border:none;'></td>\n")
                
                f.write("    <td class='spacer'></td>\n")
                
                if r_label:
                    p = dir_prefixes.get(r_label, "")
                    f.write(f"    <td class='comment' colspan='{n_cols}'>{p}</td>\n")
                else:
                    f.write(f"    <td colspan='{n_cols}' style='border:none;'></td>\n")
                f.write("  </tr>\n")
                
                # Row 3: Header
                f.write("  <tr class='header'>\n")
                if l_label:
                    f.write("    <th>Workload</th>\n")
                    for t in sorted_threads:
                        f.write(f"    <th>{t} thds</th>\n")
                else:
                    f.write(f"    <td colspan='{n_cols}' style='border:none;'></td>\n")
                
                f.write("    <td class='spacer'></td>\n")
                
                if r_label:
                    f.write("    <th>Workload</th>\n")
                    for t in sorted_threads:
                        f.write(f"    <th>{t} thds</th>\n")
                else:
                    f.write(f"    <td colspan='{n_cols}' style='border:none;'></td>\n")
                f.write("  </tr>\n")
                
                # Data Rows
                workloads = set()
                if l_label:
                    workloads.update(summary_data[l_label].keys())
                if r_label:
                    workloads.update(summary_data[r_label].keys())
                
                for workload in sorted(list(workloads)):
                    f.write("  <tr>\n")
                    
                    # Left Data
                    if l_label:
                        if workload in summary_data[l_label]:
                            f.write(f"    <td>{workload}</td>\n")
                            for t in sorted_threads:
                                val = summary_data[l_label][workload].get(t, "")
                                if isinstance(val, (int, float)):
                                    f.write(f"    <td>{val:.2f}</td>\n")
                                else:
                                    f.write("    <td></td>\n")
                        else:
                            for _ in range(n_cols): f.write("    <td></td>\n")
                    else:
                        f.write(f"    <td colspan='{n_cols}' style='border:none;'></td>\n")
                    
                    f.write("    <td class='spacer'></td>\n")
                    
                    # Right Data
                    if r_label:
                        if workload in summary_data[r_label]:
                            f.write(f"    <td>{workload}</td>\n")
                            for t in sorted_threads:
                                val = summary_data[r_label][workload].get(t, "")
                                if isinstance(val, (int, float)):
                                    f.write(f"    <td>{val:.2f}</td>\n")
                                else:
                                    f.write("    <td></td>\n")
                        else:
                            for _ in range(n_cols): f.write("    <td></td>\n")
                    else:
                        f.write(f"    <td colspan='{n_cols}' style='border:none;'></td>\n")
                        
                    f.write("  </tr>\n")
                
                # Empty row between pairs
                f.write(f"  <tr><td colspan='{2*n_cols + 1}' style='border:none; height:20px;'></td></tr>\n")
        
        if max_len > 0:
            f.write("</table>\n")
        
        # New Per-Workload Tables
        all_workloads = set()
        for d in summary_data:
            all_workloads.update(summary_data[d].keys())
        
        sorted_workloads = sorted(list(all_workloads))
        
        f.write("<br><hr><br>\n")
        
        for workload in sorted_workloads:
            f.write("<table>\n")
            
            # Header
            f.write("  <tr class='header'>\n")
            f.write(f"    <th>{workload}</th>\n")
            for t in sorted_threads:
                f.write(f"    <th>{t} thds</th>\n")
            f.write("  </tr>\n")
            
            # Group by label to merge repeating configs
            grouped_data = {}

            for dir_label in summary_data.keys():
                if workload in summary_data[dir_label]:
                    # Determine DB Type
                    db_type = "Other"
                    if "innodb" in dir_label.lower():
                        db_type = "InnoDB"
                    elif "rocksdb" in dir_label.lower():
                        db_type = "RocksDB"
                    
                    # Generate Label
                    label = dir_label
                    ram_val = 0
                    threads_val = 0
                    
                    # Try to match pattern ..._64G_32Th
                    match = re.search(r'_(\d+)G_(\d+)Th(?:$|_)', dir_label)
                    if match:
                        ram = match.group(1)
                        threads = match.group(2)
                        ram_val = int(ram)
                        threads_val = int(threads)
                        config_str = f"{ram} GB RAM, {threads} threads"
                        
                        if db_type != "Other":
                            label = f"{db_type} {config_str}"
                        else:
                            label = config_str
                    
                    if label not in grouped_data:
                        grouped_data[label] = {
                            'db_type': db_type,
                            'ram': ram_val,
                            'threads': threads_val,
                            'values': {}
                        }
                    
                    # Collect values for each thread count
                    for t in sorted_threads:
                        val = summary_data[dir_label][workload].get(t, "")
                        if isinstance(val, (int, float)):
                            if t not in grouped_data[label]['values']:
                                grouped_data[label]['values'][t] = []
                            grouped_data[label]['values'][t].append(val)

            rows_data = []
            for label, info in grouped_data.items():
                # Calculate averages
                averaged_values = {}
                for t, vals in info['values'].items():
                    if vals:
                        averaged_values[t] = sum(vals) / len(vals)
                
                rows_data.append({
                    'db_type': info['db_type'],
                    'label': label,
                    'ram': info['ram'],
                    'threads': info['threads'],
                    'data': averaged_values
                })
            
            # Sort rows: InnoDB first, then RocksDB, then Other. Secondary sort by RAM, then Threads.
            def sort_key(item):
                db_order = {'InnoDB': 0, 'RocksDB': 1, 'Other': 2}
                return (db_order.get(item['db_type'], 2), item['ram'], item['threads'], item['label'])
            
            rows_data.sort(key=sort_key)
            
            # Pre-calculate comparisons
            comparison_map = {}
            for item in rows_data:
                if item['db_type'] in ['InnoDB', 'RocksDB']:
                    key = (item['ram'], item['threads'])
                    if key not in comparison_map:
                        comparison_map[key] = {}
                    comparison_map[key][item['db_type']] = item['data']

            for item in rows_data:
                label = item['label']
                data = item['data']
                db_type = item['db_type']
                ram = item['ram']
                threads = item['threads']
                
                f.write("  <tr>\n")
                f.write(f"    <td>{label}</td>\n")
                
                for t in sorted_threads:
                    val = data.get(t, "")
                    style = ""
                    
                    # Determine color
                    if isinstance(val, (int, float)) and db_type in ['InnoDB', 'RocksDB']:
                        key = (ram, threads)
                        if key in comparison_map:
                            other_type = 'RocksDB' if db_type == 'InnoDB' else 'InnoDB'
                            if other_type in comparison_map[key]:
                                other_val = comparison_map[key][other_type].get(t)
                                if isinstance(other_val, (int, float)) and other_val > 0:
                                    diff = (val - other_val) / other_val
                                    max_diff = 0.5 # 50% difference for max saturation
                                    
                                    if diff > 0:
                                        # Green gradient
                                        ratio = min(abs(diff) / max_diff, 1.0)
                                        # White (255,255,255) to Green (87, 187, 138)
                                        r = int(255 + (87 - 255) * ratio)
                                        g = int(255 + (187 - 255) * ratio)
                                        b = int(255 + (138 - 255) * ratio)
                                        style = f" style='background-color: #{r:02x}{g:02x}{b:02x};'"
                                    elif diff < 0:
                                        # Red gradient
                                        ratio = min(abs(diff) / max_diff, 1.0)
                                        # White (255,255,255) to Red (224, 102, 102)
                                        r = int(255 + (224 - 255) * ratio)
                                        g = int(255 + (102 - 255) * ratio)
                                        b = int(255 + (102 - 255) * ratio)
                                        style = f" style='background-color: #{r:02x}{g:02x}{b:02x};'"

                    if isinstance(val, (int, float)):
                        f.write(f"    <td{style}>{val:.2f}</td>\n")
                    else:
                        f.write("    <td></td>\n")
                f.write("  </tr>\n")
            
            f.write("</table>\n<br><br>\n")
            
        f.write("</body>\n</html>")
                
    print(f"\nSummary HTML written to: {os.path.abspath(html_filename)}")

if __name__ == "__main__":
    main()
