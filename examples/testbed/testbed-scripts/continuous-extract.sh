while true;
do
rsync -avz backuplille:jobs/* $1 && python generateAll.py $1;
echo "Sleeping"
sleep 900
done;
