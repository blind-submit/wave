import pickle

def get_df(func):
    file_path = f'./data/{func}.pkl'

    # Check if the dataframe has already been computed
    with open(file_path, 'rb') as file:
        df = pickle.load(file)
        print(f"The file {file_path} was loaded successfully.")
    return df


def main():
    FUNCS = [
            "GeLU",
            "Sigmoid",
            "Tanh",
            "SiLU",
            "Softplus",
            "SELU",
            "Mish",
            "Exponential",
            "Reciprocal"
        ]
    for func in FUNCS:
        df = get_df(func)
        df.to_csv("csv"+func+".csv", sep=',')

if __name__ == "__main__":
    main()