total_clients=15

echo "Starting to launch $total_clients concurrent clients..."

for ((i=1; i<=total_clients; i++))
do
   ./bin/client &
done

wait
echo "All $total_clients clients finished successfully."
