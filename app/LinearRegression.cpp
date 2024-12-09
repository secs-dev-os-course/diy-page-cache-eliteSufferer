#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include "CacheAPI.h" // Подключаем ваш API для работы с кэшем

// Функция для выполнения линейной регрессии
void LinearRegression(int dataSize, int repetitions, int fdX, int fdY) {
    // Генерация случайных данных и запись в файлы
    std::vector<double> X(dataSize);
    std::vector<double> Y(dataSize);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist(0.0, 100.0);

    // Предположим, что истинное уравнение - Y = 2.5 * X + 10 + noise
    for (int i = 0; i < dataSize; ++i) {
        X[i] = dist(gen);
        Y[i] = 2.5 * X[i] + 10 + dist(gen) * 0.1;  // Добавляем шум
    }

    // Записываем данные в файлы через кэш
    lab2_write(fdX, X.data(), dataSize * sizeof(double));
    lab2_write(fdY, Y.data(), dataSize * sizeof(double));

    auto start = std::chrono::high_resolution_clock::now();
    double final_a = 0.0, final_b = 0.0;  // Сохраняем последние значения коэффициентов

    for (int r = 0; r < repetitions; ++r) {
        // Считываем данные из файлов через кэш
        lab2_lseek(fdX, 0, SEEK_SET);
        lab2_lseek(fdY, 0, SEEK_SET);
        lab2_read(fdX, X.data(), dataSize * sizeof(double));
        lab2_read(fdY, Y.data(), dataSize * sizeof(double));

        // Вычисление параметров линейной регрессии
        double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;

        for (int i = 0; i < dataSize; ++i) {
            sumX += X[i];
            sumY += Y[i];
            sumXY += X[i] * Y[i];
            sumX2 += X[i] * X[i];
        }

        // Вычисляем коэффициенты a и b
        final_a = (dataSize * sumXY - sumX * sumY) / (dataSize * sumX2 - sumX * sumX);
        final_b = (sumY - final_a * sumX) / dataSize;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    std::cout << "LinearRegression completed " << repetitions << " times on the set of "
              << dataSize << " points.\n";
    std::cout << "Final equation: Y = " << final_a << " * X + " << final_b << '\n';
    std::cout << "Execution time: " << duration.count() << " secs\n";
}

// Функция для запуска линейной регрессии через API кэша
void runLinearRegression(int dataSize, int repetitions, int numRepetitionsPerLoader) {
    // Открываем файлы для X и Y через кэш
    int fdX = lab2_open("X.data");
    int fdY = lab2_open("Y.data");
    if (fdX == -1 || fdY == -1) {
        std::cerr << "Ошибка: не удалось открыть файлы.\n";
        return;
    }

    // Выполняем линейную регрессию заданное количество раз
    for (int i = 0; i < numRepetitionsPerLoader; ++i) {
        std::cout << "Loader iteration " << i + 1 << " of " << numRepetitionsPerLoader << "\n";
        LinearRegression(dataSize, repetitions, fdX, fdY);
    }

    // Закрываем файлы
    lab2_close(fdX);
    lab2_close(fdY);

    std::cout << "Finished loader with dataSize="
              << dataSize << ", repetitions=" << repetitions
              << ", and numRepetitionsPerLoader=" << numRepetitionsPerLoader << "\n";
}
