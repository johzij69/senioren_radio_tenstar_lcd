from flask import Flask, jsonify, send_from_directory
from pathlib import Path
import os

app = Flask(__name__)
DATA_ROOT = Path(os.environ.get("OTA_DATA_ROOT", "/srv/ota"))
WEBRADIO_DIR = DATA_ROOT / "webradio"


def read_current_version() -> str:
    version_file = WEBRADIO_DIR / "version.txt"
    if version_file.exists():
        return version_file.read_text(encoding="utf-8").strip()
    return os.environ.get("OTA_VERSION", "0.0.0")


@app.get("/")
def health():
    return jsonify({
        "service": "ota-server",
        "status": "ok",
        "version": read_current_version(),
    })


@app.get("/webradio/version.json")
def version_manifest():
    return jsonify({
        "version": read_current_version(),
        "bin": "firmware.bin",
        "product": "senioren_radio_tenstar_lcd",
    })


@app.get("/webradio/firmware.bin")
def firmware_bin():
    return send_from_directory(WEBRADIO_DIR, "firmware.bin", as_attachment=False)


@app.get("/webradio/<path:filename>")
def webradio_files(filename: str):
    return send_from_directory(WEBRADIO_DIR, filename, as_attachment=False)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)
