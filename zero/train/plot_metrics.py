"""Plots training curves from train.py runs' metrics.jsonl files.

One panel per metric, with training (every --log_every steps, averaged over
those steps) and validation (after each epoch) for each run, against epochs.

  uv run python plot_metrics.py --runs=../data/runs/bootstrap-6x32,../data/runs/bootstrap-6x64 --out=../../analysis/images/bootstrap-training.svg
"""

import json
import os

from absl import app
from absl import flags
import matplotlib

matplotlib.use("svg")
import matplotlib.pyplot as plt  # pylint: disable=g-import-not-at-top

_RUNS = flags.DEFINE_list(
    "runs", None, "Run directories (train.py --out) to plot.", required=True)
_OUT = flags.DEFINE_string(
    "out", None, "Image to write (.svg or .png).", required=True)
_SKIP_STEPS = flags.DEFINE_integer(
    "skip_steps", 300,
    "Training points up to this step (warmup, whose losses are off the "
    "scale) aren't plotted.")

# (metric, panel title).
_PANELS = [
    ("policy_loss", "Policy loss"),
    ("outcome_loss", "Outcome loss"),
    ("margin_error", "Margin error (points)"),
    ("policy_acc", "Policy accuracy (top move)"),
    ("outcome_acc", "Outcome accuracy"),
]
# Categorical colors, one per run, in a fixed order.
_COLORS = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]
_TEXT = "#3d3d3a"
_GRID = "#e4e3dd"


def read_metrics(run: str) -> dict[str, list[dict]]:
    """Returns the run's metrics records, split into "train" and "val"."""
    records = {"train": [], "val": []}
    with open(os.path.join(run, "metrics.jsonl")) as f:
        for line in f:
            record = json.loads(line)
            records[record["split"]].append(record)
    return records


def main(argv: list[str]) -> None:
    del argv  # Unused.
    plt.rcParams.update({
        "font.size": 9, "text.color": _TEXT, "axes.labelcolor": _TEXT,
        "xtick.color": _TEXT, "ytick.color": _TEXT, "axes.edgecolor": _GRID,
        "svg.fonttype": "none",
    })
    fig, axes = plt.subplots(2, 3, figsize=(10, 6), layout="constrained")
    axes = axes.flatten()
    for color, run in zip(_COLORS, _RUNS.value):
        name = os.path.basename(os.path.normpath(run))
        records = read_metrics(run)
        # Validation runs at the end of each epoch.
        steps_per_epoch = records["val"][0]["step"]
        train = [r for r in records["train"] if r["step"] > _SKIP_STEPS.value]
        train_epochs = [r["step"] / steps_per_epoch for r in train]
        val_epochs = [r["step"] / steps_per_epoch for r in records["val"]]
        for ax, (metric, _) in zip(axes, _PANELS):
            ax.plot(train_epochs, [r[metric] for r in train], color=color,
                    linewidth=1.5, alpha=0.55, label=f"{name} train")
            ax.plot(val_epochs, [r[metric] for r in records["val"]],
                    color=color, linewidth=2, marker="o", markersize=5,
                    label=f"{name} validation")
        axes[5].plot(train_epochs, [r["lr"] for r in train], color=color,
                     linewidth=2)
    for ax, (_, title) in zip(axes, _PANELS + [(None, "Learning rate")]):
        ax.set_title(title, loc="left", fontsize=10)
        ax.set_xlabel("Epoch")
        ax.grid(color=_GRID, linewidth=0.8)
        ax.tick_params(length=0)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="outside upper center",
               ncol=len(labels), frameon=False)
    fig.savefig(_OUT.value, facecolor="white")
    print(f"Wrote {_OUT.value}")


if __name__ == "__main__":
    app.run(main)
