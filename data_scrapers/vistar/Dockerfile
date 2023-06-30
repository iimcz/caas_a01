FROM python:3

# Install Tesseracr OCR needed for the app
RUN /bin/sh -c set -eux; apt-get update; apt-get install -y --no-install-recommends tesseract-ocr; rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/app

COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

CMD [ "python", "-u", "./vistar_data.py" ]
