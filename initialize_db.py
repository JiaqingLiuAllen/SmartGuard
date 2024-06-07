import sqlite3 as sql

# Establish a connection to the SQLite database file
db_connection = sql.connect('fall.db')

# Create the FALL table with necessary columns
with db_connection:
    db_connection.execute('''
        CREATE TABLE FALL (
            id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
            Timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            name TEXT,
            worker TEXT
        );
    ''')

print("Database and table created successfully.")