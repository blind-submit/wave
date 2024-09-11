import pickle
import argparse


def get_df(func):
    file_path = f'./data/{func}.pkl'

    # Check if the dataframe has already been computed
    with open(file_path, 'rb') as file:
        df = pickle.load(file)
        print(f"The file {file_path} was loaded successfully.")
    return df


def main():
    parser = argparse.ArgumentParser(description="Print dataframe for select function.")
    parser.add_argument("func")
    args = parser.parse_args()

    df = get_df(args.func)
    print(df.to_string())


if __name__ == "__main__":
    main()