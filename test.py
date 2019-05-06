filepath = 'throughput.txt'  
count = 0
sum = 0
with open(filepath, 'r') as infile:
    for line in infile:
        count+=1
        sum+=int(line)
print(sum/count)