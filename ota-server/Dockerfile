FROM python:3.12-slim

WORKDIR /app
COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

COPY app.py ./
ENV OTA_DATA_ROOT=/srv/ota
ENV OTA_PORT=8000
EXPOSE 8000

CMD ["sh", "-c", "gunicorn --bind 0.0.0.0:${OTA_PORT} --workers 2 --threads 2 --timeout 30 app:app"]
