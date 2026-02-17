import matplotlib.pyplot as plt
import os
import sys
import re
import argparse


def parse_summary_tables(html_file):
    """Parse colored summary tables from the HTML file.

    Identifies tables where cells use inline background-color styles,
    which are the summary/comparison tables at the end of the report.
    Returns a dict keyed by workload name, each containing 'threads' and 'data'.
    """
    with open(html_file, 'r') as f:
        content = f.read()

    table_blocks = re.findall(r'<table>(.*?)</table>', content, re.DOTALL)

    workloads = {}
    for table_html in table_blocks:
        if 'background-color' not in table_html:
            continue

        header_row_m = re.search(r"<tr class='header'>(.*?)</tr>", table_html, re.DOTALL)
        if not header_row_m:
            continue
        header_html = header_row_m.group(1)

        ths = re.findall(r'<th>(.*?)</th>', header_html)
        if not ths:
            continue
        workload_name = ths[0]
        threads = [re.sub(r'\s*thds?', '', t).strip() for t in ths[1:]]

        data = {}
        data_rows = re.findall(r'<tr>\s*(.*?)\s*</tr>', table_html, re.DOTALL)
        for row_html in data_rows:
            tds = re.findall(r"<td[^>]*>(.*?)</td>", row_html)
            if len(tds) < 2:
                continue
            label = tds[0].strip()
            try:
                values = [float(v.strip()) for v in tds[1:]]
            except ValueError:
                continue
            data[rename_label(label)] = values

        display_name = WORKLOAD_NAMES.get(workload_name, workload_name)
        workloads[display_name] = {'threads': threads, 'data': data}

    return workloads


SIZE_LABELS = {32: 'Small', 64: 'Medium', 128: 'Large', 256: 'Extra Large'}
ENGINE_NAMES = {'innodb': 'InnoDB', 'rocksdb': 'MyRocks'}
WORKLOAD_NAMES = {
    'READ_ONLY_TRX': 'Read Only Workload',
    'READ_WRITE_TRX': 'Mixed Read/Write Workload',
    'WRITE_ONLY_TRX': 'Write Only Workload',
}


def rename_label(label):
    """Rename HTML label to display format.

    e.g. 'InnoDB 64 GB RAM, 32 threads' -> 'InnoDB Medium Instance (64 GB, 32 vCPU)'
         'RocksDB 128 GB RAM, 64 threads' -> 'MyRocks Large Instance (128 GB, 64 vCPU)'
    """
    m = re.match(r'(\w+)\s+(\d+)\s*GB\s*RAM,\s*(\d+)\s*threads?', label, re.IGNORECASE)
    if not m:
        return label
    engine_raw, ram, vcpus = m.group(1), int(m.group(2)), int(m.group(3))
    engine = ENGINE_NAMES.get(engine_raw.lower(), engine_raw)
    size = SIZE_LABELS.get(ram, f'{ram}GB')
    return f'{engine} {size} Instance ({ram} GB, {vcpus} vCPU)'


INNODB_COLORS = ['#1f77b4', '#17becf', '#2ca02c']
ROCKS_COLORS = ['#d62728', '#ff7f0e', '#9467bd']
MARKERS = ['o', 's', '^', 'D', 'v', 'p']


def print_tables(workloads):
    """Print parsed workload data as formatted tables."""
    for workload_name, info in workloads.items():
        threads = info['threads']
        data = info['data']

        label_width = max(len(l) for l in data)
        col_width = max(max(len(t) for t in threads), 12)

        header = f"{'Configuration':<{label_width}}  " + "  ".join(f"{t + ' thds':>{col_width}}" for t in threads)
        print(f"\n  {workload_name}")
        print(f"  {'=' * len(header)}")
        print(f"  {header}")
        print(f"  {'-' * len(header)}")
        for label, values in data.items():
            cols = "  ".join(f"{v:>{col_width},.2f}" for v in values)
            print(f"  {label:<{label_width}}  {cols}")
        print()


def generate_graph(title, data, threads, output_filename):
    plt.figure(figsize=(12, 8))

    innodb_idx = 0
    rocks_idx = 0
    for label, values in data.items():
        if 'innodb' in label.lower() or 'inno' in label.lower():
            color = INNODB_COLORS[innodb_idx % len(INNODB_COLORS)]
            marker = MARKERS[innodb_idx % len(MARKERS)]
            innodb_idx += 1
        else:
            color = ROCKS_COLORS[rocks_idx % len(ROCKS_COLORS)]
            marker = MARKERS[rocks_idx % len(MARKERS)]
            rocks_idx += 1
        plt.plot(threads, values, label=label, color=color, marker=marker, linestyle='-')

    plt.title(f'{title} Performance (QPS)', fontsize=16)
    plt.xlabel('Threads', fontsize=12)
    plt.ylabel('Queries Per Second (QPS)', fontsize=12)
    plt.grid(True, which='both', linestyle='--', linewidth=0.5)
    plt.legend(title='Configuration', fontsize=10)

    plt.savefig(output_filename, dpi=300, bbox_inches='tight')
    print(f"Graph saved: {os.path.abspath(output_filename)}")
    plt.close()


def main():
    parser = argparse.ArgumentParser(
        description='Generate benchmark graphs from sysbench summary HTML files.')
    parser.add_argument('html_file', help='Path to the sysbench summary HTML file')
    parser.add_argument('-o', '--output-dir', default=None,
                        help='Output directory for graphs (default: same directory as input file)')
    args = parser.parse_args()

    if not os.path.isfile(args.html_file):
        print(f"Error: file not found: {args.html_file}", file=sys.stderr)
        sys.exit(1)

    output_dir = args.output_dir or os.path.dirname(os.path.abspath(args.html_file))
    os.makedirs(output_dir, exist_ok=True)

    workloads = parse_summary_tables(args.html_file)
    if not workloads:
        print("No summary tables (with background-color cells) found in the HTML file.",
              file=sys.stderr)
        sys.exit(1)

    print_tables(workloads)

    for workload_name, info in workloads.items():
        safe_name = re.sub(r'[^\w]+', '_', workload_name.lower()).strip('_')
        output_path = os.path.join(output_dir, f'{safe_name}_benchmark_graph.png')
        generate_graph(workload_name, info['data'], info['threads'], output_path)

    print(f"\nGenerated {len(workloads)} graph(s) in: {output_dir}")


if __name__ == "__main__":
    main()
