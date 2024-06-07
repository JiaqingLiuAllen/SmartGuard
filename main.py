import asyncio
import os
import smtplib
import sqlite3 as sql
import string
import time
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText

from bleak import BleakScanner
from termcolor import colored

# Connect to the database and create a cursor
db_connection = sql.connect("fall.db")
db_cursor = db_connection.cursor()

def send_email_alert(subject, message, recipient_email, sender_email, sender_password):
    # Setup email server and send an email alert
    email_server = smtplib.SMTP("smtp.gmail.com", 587)
    email_server.starttls()
    try:
        email_server.login(sender_email, sender_password)  # Use a generated app password
        email_message = MIMEMultipart()
        email_message["From"] = sender_email
        email_message["To"] = recipient_email
        email_message["Subject"] = subject
        email_message.attach(MIMEText(message, "plain"))
        email_server.send_message(email_message)
        print("Email sent successfully!")
    except smtplib.SMTPAuthenticationError:
        print("Authentication failed. Check your email settings or app password.")
    finally:
        email_server.quit()

# Create the FALL table if not already exists
with db_connection:
    db_connection.execute('''
        CREATE TABLE IF NOT EXISTS FALL (
            id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
            Timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            name TEXT,
            worker TEXT
        );
    ''')

found = 0
iterations = 0

# Clear the terminal screen
os.system("cls" if os.name == "nt" else "clear")
print(colored('SmartGuard - Elderly Fall Detection System', 'green'))
print("\nScanning...\n")

async def run_scanner():
    global found, iterations
    while True:
        print("#" + str(iterations))

        devices = await BleakScanner.discover()

        for device in devices:
            device_name = device.name

            if device_name:
                print(colored(f"    {device_name}, {device.address}", "yellow"))

                if "Fall" in device_name:
                    found = 1
                    send_email_alert(
                        "SmartGuard Automatic Alert",
                        "Alert! SmartGuard has detected a fall. Please check immediately.",
                        "recipient@example.com",
                        "sender@example.com",
                        "your-email-password"
                    )
                    print(colored("Fall detected", "red"))

                    # Handle the device information and update the database accordingly
                    sql_query = "SELECT * FROM FALL WHERE name=?"
                    db_cursor.execute(sql_query, (device_name,))
                    records = db_cursor.fetchall()

                    if not records:
                        print("Adding record: " + device_name)
                        worker_name = device_name.split("-")[1] if len(device_name.split("-")) > 1 else "Unknown"
                        insert_sql = "INSERT INTO FALL (name, worker) values (?, ?)"
                        with db_connection:
                            db_connection.execute(insert_sql, (device_name, worker_name))
                        time.sleep(5)
                    else:
                        print(colored("This fall was already in the database", "green"))

        if found == 0:
            print("\nNo falls detected")

        iterations += 1
        time.sleep(1)

asyncio.run(run_scanner())
