FROM python:3

WORKDIR /usr/src/app

COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

ENV CAD_DATA_TAB=2

CMD [ "python", "-u", "./cad_data.py" ]
