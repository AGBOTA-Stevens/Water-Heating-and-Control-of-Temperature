#!/usr/bin/env python3
"""Capture une experience ESP32 et genere les figures PNG du memoire."""

import argparse
import csv
import sys
import time
from datetime import datetime, timedelta
from pathlib import Path

import matplotlib.dates as mdates
import matplotlib.pyplot as plt
import serial


DATA_FIELDS = (
    "record",
    "elapsed_s",
    "phase",
    "temperature_c",
    "target_c",
    "pid_setpoint_c",
    "output",
    "target_error_c",
    "pid_error_c",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture les donnees serie et genere les graphes de l'experience."
    )
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Port serie de l'ESP32")
    parser.add_argument("--baud", type=int, default=115200, help="Vitesse serie")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("results"),
        help="Dossier de sortie des CSV et PNG",
    )
    return parser.parse_args()


def duration(rows: list[dict], phase: str) -> float | None:
    phase_rows = [row for row in rows if row["phase"] == phase]
    if not phase_rows:
        return None
    return phase_rows[-1]["elapsed_s"] - phase_rows[0]["elapsed_s"]


def make_graphs(rows: list[dict], output_dir: Path) -> None:
    if len(rows) < 2:
        raise ValueError("Il faut au moins deux mesures valides pour tracer les graphes.")

    start_datetime = datetime.fromisoformat(rows[0]["timestamp"])
    time_s = [row["elapsed_s"] for row in rows]
    timestamps = [start_datetime + timedelta(seconds=value) for value in time_s]
    temperature = [row["temperature_c"] for row in rows]
    target = [row["target_c"] for row in rows]
    target_error = [row["target_error_c"] for row in rows]
    pid_error = [row["pid_error_c"] for row in rows]
    output = [row["output"] for row in rows]
    reached_index = next(
        (index for index, value in enumerate(target_error) if abs(value) <= 0.5),
        None,
    )
    reached_time = time_s[reached_index] if reached_index is not None else None
    mae = sum(abs(value) for value in pid_error) / len(pid_error)
    max_error = max(abs(value) for value in pid_error)
    heating_time = duration(rows, "heating")
    cooling_time = duration(rows, "cooling")
    total_time = time_s[-1] - time_s[0]
    date_formatter = mdates.DateFormatter("%d/%m/%Y %H:%M:%S")

    figure, axis = plt.subplots(figsize=(10, 5.8))
    axis.plot(timestamps, temperature, label="Temperature mesuree", linewidth=2)
    axis.plot(timestamps, target, "--", label="Temperature cible", linewidth=1.8)
    if reached_time is not None:
        axis.axvline(
            start_datetime + timedelta(seconds=reached_time),
            color="tab:green",
            linestyle=":",
            label=f"Atteinte a {reached_time:.1f} s (±0,5 °C)",
        )
    axis.set_xlabel("Date et heure (JJ/MM/AAAA HH:MM:SS)")
    axis.set_ylabel("Temperature (°C)")
    axis.set_title("Temps de chauffe et atteinte de la temperature cible")
    axis.xaxis.set_major_formatter(date_formatter)
    axis.grid(True, alpha=0.3)
    axis.legend()
    figure.autofmt_xdate()
    figure.tight_layout()
    figure.savefig(output_dir / "temperature_vs_time.png", dpi=300)
    plt.close(figure)

    figure, axes = plt.subplots(2, 1, figsize=(10, 7), sharex=True)
    axes[0].plot(timestamps, pid_error, color="tab:orange", linewidth=2)
    axes[0].axhline(0, color="black", linewidth=0.8)
    axes[0].axhline(0.5, color="tab:green", linestyle="--", linewidth=1)
    axes[0].axhline(-0.5, color="tab:green", linestyle="--", linewidth=1)
    axes[0].set_ylabel("Erreur (°C)")
    axes[0].set_title(
        f"Precision du PID — erreur moyenne: {mae:.2f} °C, "
        f"erreur maximale: {max_error:.2f} °C"
    )
    axes[0].grid(True, alpha=0.3)
    axes[1].plot(timestamps, output, color="tab:blue", linewidth=2)
    axes[1].set_xlabel("Date et heure (JJ/MM/AAAA HH:MM:SS)")
    axes[1].set_ylabel("Commande PID (PWM)")
    axes[1].set_ylim(bottom=0)
    axes[1].xaxis.set_major_formatter(date_formatter)
    axes[1].grid(True, alpha=0.3)
    figure.autofmt_xdate()
    figure.tight_layout()
    figure.savefig(output_dir / "pid_precision.png", dpi=300)
    plt.close(figure)

    cooling_rows = [row for row in rows if row["phase"] == "cooling"]
    figure, axis = plt.subplots(figsize=(10, 5.8))
    if cooling_rows:
        cooling_timestamps = [
            start_datetime + timedelta(seconds=row["elapsed_s"])
            for row in cooling_rows
        ]
        axis.plot(
            cooling_timestamps,
            [row["temperature_c"] for row in cooling_rows],
            color="tab:cyan",
            linewidth=2,
            label="Temperature pendant le refroidissement",
        )
        axis.axhline(
            30.0,
            color="tab:red",
            linestyle="--",
            label="Temperature ambiante (30 °C)",
        )
        axis.set_title(
            f"Chute de temperature apres chauffage — duree: {cooling_time:.1f} s"
        )
    else:
        axis.text(
            0.5,
            0.5,
            "Aucune mesure de refroidissement recue.",
            ha="center",
            va="center",
            transform=axis.transAxes,
        )
        axis.set_title("Chute de temperature apres chauffage")
    axis.set_xlabel("Date et heure (JJ/MM/AAAA HH:MM:SS)")
    axis.set_ylabel("Temperature (°C)")
    axis.xaxis.set_major_formatter(date_formatter)
    axis.grid(True, alpha=0.3)
    if cooling_rows:
        axis.legend()
    figure.autofmt_xdate()
    figure.tight_layout()
    figure.savefig(output_dir / "cooling_vs_time.png", dpi=300)
    plt.close(figure)

    metrics = [
        ("Chauffage", heating_time),
        ("Refroidissement", cooling_time),
        ("Cycle total", total_time),
        ("Atteinte cible", reached_time),
    ]
    valid_metrics = [(label, value) for label, value in metrics if value is not None]
    figure, axis = plt.subplots(figsize=(9, 5.5))
    axis.bar(
        [label for label, _ in valid_metrics],
        [value for _, value in valid_metrics],
        color=["tab:orange", "tab:cyan", "tab:blue", "tab:green"][: len(valid_metrics)],
    )
    axis.set_ylabel("Duree (s)")
    axis.set_title(
        "Performances du cycle\n"
        f"Erreur absolue moyenne: {mae:.2f} °C — maximale: {max_error:.2f} °C"
    )
    axis.grid(axis="y", alpha=0.3)
    for index, (_, value) in enumerate(valid_metrics):
        axis.text(index, value, f"{value:.1f} s", ha="center", va="bottom")
    figure.tight_layout()
    figure.savefig(output_dir / "performance_summary.png", dpi=300)
    plt.close(figure)

    print(f"Mesures capturees : {len(rows)}")
    print(
        "Temps d'atteinte (bande ±0,5 °C) : "
        + (f"{reached_time:.2f} s" if reached_time is not None else "non atteint")
    )
    print(f"Erreur absolue moyenne : {mae:.2f} °C")
    print(f"Erreur absolue maximale : {max_error:.2f} °C")
    print(
        "Duree de chauffage : "
        + (f"{heating_time:.2f} s" if heating_time is not None else "non disponible")
    )
    print(
        "Duree de refroidissement : "
        + (
            f"{cooling_time:.2f} s"
            if cooling_time is not None
            else "non disponible"
        )
    )


def capture(args: argparse.Namespace) -> list[dict]:
    rows: list[dict] = []
    recording = False
    experiment_start_datetime: datetime | None = None
    print(f"Ecoute de {args.port}. Appuyez sur Ctrl+C pour arreter.")
    try:
        with serial.Serial(args.port, args.baud, timeout=1) as connection:
            time.sleep(2)
            while True:
                line = connection.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                if line == "EXPERIMENT_BEGIN":
                    rows = []
                    recording = True
                    experiment_start_datetime = datetime.now().replace(microsecond=0)
                    print("Experience demarree.")
                elif line == "EXPERIMENT_END":
                    if recording:
                        return rows
                    print("Fin d'experience ignoree : aucun debut recu.")
                elif recording and line.startswith("DATA,"):
                    values = next(csv.reader([line]))
                    if len(values) != len(DATA_FIELDS):
                        raise ValueError(
                            f"Ligne DATA invalide ({len(values)} champs au lieu de "
                            f"{len(DATA_FIELDS)}) : {line}"
                        )
                    if values[1] == "elapsed_s":
                        continue
                    elapsed_s = float(values[1])
                    rows.append(
                        {
                            "record": values[0],
                            "elapsed_s": elapsed_s,
                            "timestamp": (
                                experiment_start_datetime
                                + timedelta(seconds=elapsed_s)
                            ).isoformat(sep=" ")
                            if experiment_start_datetime is not None
                            else datetime.now().isoformat(sep=" "),
                            "phase": values[2],
                            **{
                                field: float(value)
                                for field, value in zip(DATA_FIELDS[3:], values[3:])
                            },
                        }
                    )
    except KeyboardInterrupt:
        if rows:
            print(
                "\nCapture arretee manuellement : les mesures deja recues "
                "seront utilisees pour generer les graphes."
            )
            return rows
        raise


def main() -> None:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    try:
        rows = capture(args)
    except KeyboardInterrupt:
        print(
            "\nCapture interrompue avant la reception d'une mesure.",
            file=sys.stderr,
        )
        return
    except serial.SerialException as error:
        raise SystemExit(f"Impossible d'ouvrir le port serie : {error}") from error

    if not rows:
        raise SystemExit("Aucune mesure valide recue pendant l'experience.")

    csv_path = args.output_dir / "experiment_data.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    make_graphs(rows, args.output_dir)
    print(f"Fichiers generes dans : {args.output_dir.resolve()}")


if __name__ == "__main__":
    main()
