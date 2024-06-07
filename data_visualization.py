import sqlite3 as sql
import pandas as pd
import matplotlib.pyplot as plt
from termcolor import colored
import os

# Connect to the SQLite database
connection = sql.connect('fall.db')

# Clear the terminal screen
os.system('clear')

# Header for the system
print(colored('SmartGuard - Elderly Fall Detection System', 'green'))
print("\nPrevious falls\n")

# Query and display previous falls
with connection:
    fall_records = connection.execute("SELECT Timestamp, worker FROM FALL")
    for record in fall_records:
        print(colored(f"    {record[0]}", 'yellow'), colored(record[1], 'yellow'))

# Generate and display a bar chart of falls per worker
query = "SELECT worker, COUNT(*) AS Count FROM FALL GROUP BY worker"
fall_data = pd.read_sql(query, connection)

plt.bar(fall_data.worker, fall_data.Count)
plt.title("Worker Falls Report")
plt.xlabel("Worker")
plt.ylabel("Falls")
plt.savefig('chart.png')
plt.show()
