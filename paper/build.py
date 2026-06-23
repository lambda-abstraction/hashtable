#!/usr/bin/env python3
import sys
import pathlib
import argparse
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

sns.set_theme(style="whitegrid", context="paper")


def generate_table(df, operation, caption, label, selected_N=None):
    sub = df[df.Operation == operation].copy()
    if selected_N is not None:
        sub = sub[sub["N"].isin(selected_N)]
    sub = sub.sort_values(["N", "Strategy"])
    lines = []
    for _, row in sub.iterrows():
        mean = f"{row['mean']:.2f}"
        stddev = f"{row['stddev']:.2f}"
        minv = row["min"]
        maxv = row["max"]
        lines.append(
            f"    [{row['Strategy']}], [{row['N']}], [{mean}], [{stddev}], [{minv:.2f}], [{maxv:.2f}],"
        )
    header = "table.header([Политика], [N], [Среднее (нс)], [σ], [Мин], [Макс])"
    body = "\n".join(lines)
    return f"""#figure(
  table(
    columns: (1fr, auto, auto, auto, auto, auto),
    {header},
{body}
  ),
  caption: "{caption}",
  kind: "table",
  supplement: "Таблица"
) <{label}>
"""


def plot_data(df, operation, distrib, out_dir):
    sub = df[(df.Operation == operation) & (df.Distribution == distrib)]
    if sub.empty:
        return
    plt.figure(figsize=(8, 5))
    sns.lineplot(
        data=sub,
        x="N",
        y="mean",
        hue="Strategy",
        marker="o",
        style="Strategy",
        dashes=False,
    )
    plt.title(f"{operation} time vs N ({distrib})")
    plt.ylabel("Mean time (ns) - log scale")
    plt.xlabel("N")
    plt.yscale("log")  # <--- логарифмическая шкала по Y
    plt.grid(True, which="both", linestyle="--", linewidth=0.5)
    plt.legend(title="Strategy")
    plt.tight_layout()
    out_path = out_dir / f"{operation.lower()}_plot_{distrib}.png"
    plt.savefig(out_path, dpi=300, bbox_inches="tight")
    plt.close()


def generate_load_plots(df_load, out_dir):
    distributions = df_load["Distribution"].unique()
    operations = df_load["Operation"].unique()
    sizes = df_load["N"].unique()

    for distrib in distributions:
        for op in operations:
            for N in sizes:
                sub = df_load[
                    (df_load.Distribution == distrib)
                    & (df_load.Operation == op)
                    & (df_load.N == N)
                ]
                if sub.empty:
                    continue
                plt.figure(figsize=(8, 5))
                sns.lineplot(
                    data=sub,
                    x="LoadFactor",
                    y="mean",
                    hue="Strategy",
                    marker="o",
                    style="Strategy",
                    dashes=False,
                )
                plt.title(f"{op} time vs Load factor (N={N}, {distrib})")
                plt.ylabel("Mean time (ns) - log scale")
                plt.xlabel("Load factor")
                plt.yscale("log")
                plt.grid(True, which="both", linestyle="--", linewidth=0.5)
                plt.legend(title="Strategy")
                plt.tight_layout()
                fname = out_dir / f"load_{op.lower()}_N{N}_{distrib}.png"
                plt.savefig(fname, dpi=300, bbox_inches="tight")
                plt.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bench", help="Path to main benchmark CSV file")
    parser.add_argument("--load", help="Path to load factor benchmark CSV file")
    parser.add_argument(
        "--table-n",
        nargs="+",
        type=int,
        help="List of N values to include in tables (e.g., 1000 10000 100000). If not set, all N are shown.",
    )
    parser.add_argument(
        "--output", default=".", help="Output directory for generated files"
    )
    args = parser.parse_args()

    if not any([args.bench, args.load]):
        parser.print_usage()
        sys.exit(1)

    out_dir = pathlib.Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.bench:
        csv_path = pathlib.Path(args.bench)
        if not csv_path.exists():
            print(f"Error: benchmark file '{csv_path}' not found", file=sys.stderr)
            sys.exit(1)
        df = pd.read_csv(csv_path)
        distributions = df["Distribution"].unique()
        selected_N = args.table_n if args.table_n else None
        for distrib in distributions:
            sub = df[df.Distribution == distrib]
            insert_typ = generate_table(
                sub,
                "Insert",
                f"Время вставки ({distrib})",
                f"tbl:insert_{distrib}",
                selected_N=selected_N,
            )
            lookup_typ = generate_table(
                sub,
                "Lookup",
                f"Время поиска ({distrib})",
                f"tbl:lookup_{distrib}",
                selected_N=selected_N,
            )
            with open(
                out_dir / f"insert_table_{distrib}.typ", "w", encoding="utf-8"
            ) as f:
                f.write(insert_typ)
            with open(
                out_dir / f"lookup_table_{distrib}.typ", "w", encoding="utf-8"
            ) as f:
                f.write(lookup_typ)
            plot_data(df, "Insert", distrib, out_dir)
            plot_data(df, "Lookup", distrib, out_dir)
        print(f"Generated tables and plots from {csv_path}")

    if args.load:
        load_path = pathlib.Path(args.load)
        if not load_path.exists():
            print(
                f"Error: load benchmark file '{load_path}' not found", file=sys.stderr
            )
            sys.exit(1)
        df_load = pd.read_csv(load_path)
        generate_load_plots(df_load, out_dir)
        print(f"Generated load factor plots from {load_path}")


if __name__ == "__main__":
    main()
