import argparse
import csv

def read_csv(name, file, peer):
    with open(file + str(peer) + ".log") as csvfile:
        reader = csv.reader(csvfile, delimiter=",")
        count = 0
        total = 0
        for row in reader:
            if row[0] == (name + "_time_ms"):
                count += 1
                total += float(row[1])
    return total / count

def create_csv(args, name):
    rows = [["n"]] + [[str(i)] for i in range(int(args.Jmin), int(args.Jmax)+1)]
    print(rows)
    for n in range(int(args.nmin), int(args.nmax)+1):
        rows[0].append(str(n))
        i = 1
        for J in range(int(args.Jmin), int(args.Jmax)+1):
            file = f"{args.dir}/n_{n}_J_{J}_{name}_peer"
            peer0_runtime = read_csv(name, file, 0)
            peer1_runtime = read_csv(name, file, 1)
            rows[i].append("%.0f" % ((peer0_runtime + peer1_runtime) / 2))
            print(f"{n=} {J=} {peer0_runtime=} {peer1_runtime=}")
            i += 1

    with open(f"{args.dir}/{name}.dat", "w") as file:
        for row in rows:
            file.write(" ".join(row)+"\n")

def main():
    parser = argparse.ArgumentParser(
                        prog='CSVMerger',
                        description='Merge the outputs of local runs')
    parser.add_argument("--nmax")
    parser.add_argument("--nmin")
    parser.add_argument("--Jmax")
    parser.add_argument("--Jmin")
    parser.add_argument("--dir")
    args = parser.parse_args()

    create_csv(args, "haar")
    create_csv(args, "bior")

if __name__ == "__main__":
    main()