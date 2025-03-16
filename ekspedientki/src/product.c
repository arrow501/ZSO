#include "product.h"



/* Global Variables*/
product_t products[MAX_PRODUCTS];
int num_products = 0;
pthread_mutex_t inventory_mutex;


#define INIT_PRODUCT(pid, pname, pprice, pstock, passist) \
    do                                                \
    {                                                 \
        products[pid].id = pid;                       \
        strcpy(products[pid].name, pname);            \
        products[pid].price = pprice;                 \
        products[pid].stock = pstock;                 \
        products[pid].needs_assistant = passist;      \
    } while (0)

void initialize_products(){
    INIT_PRODUCT(0, "Banana", 129, 45, false);
    INIT_PRODUCT(1, "Apple", 159, 50, false);
    INIT_PRODUCT(2, "Bread", 349, 32, false);
    INIT_PRODUCT(3, "Milk", 399, 40, false);
    INIT_PRODUCT(4, "Eggs", 599, 30, false);
    INIT_PRODUCT(5, "Pasta", 259, 35, false);
    INIT_PRODUCT(6, "Rice", 329, 48, false);
    INIT_PRODUCT(7, "Salt", 159, 60, false);
    INIT_PRODUCT(8, "Sugar", 289, 55, false);
    INIT_PRODUCT(9, "Chocolate", 499, 40, false);
    INIT_PRODUCT(10, "Cheese", 899, 25, false);
    INIT_PRODUCT(11, "Yogurt", 449, 30, false);
    INIT_PRODUCT(12, "Butter", 599, 28, false);
    INIT_PRODUCT(13, "Coffee", 999, 35, false);
    INIT_PRODUCT(14, "Tea", 599, 40, false);
    INIT_PRODUCT(15, "Juice", 449, 38, false);
    INIT_PRODUCT(16, "Water", 149, 70, false);
    INIT_PRODUCT(17, "Soda", 249, 60, false);
    INIT_PRODUCT(18, "Chips", 349, 45, false);
    INIT_PRODUCT(19, "Cookies", 399, 35, false);
    INIT_PRODUCT(20, "Cereal", 459, 30, false);
    INIT_PRODUCT(21, "Jam", 399, 25, false);
    INIT_PRODUCT(22, "Honey", 799, 20, false);
    INIT_PRODUCT(23, "Nuts", 699, 30, false);
    INIT_PRODUCT(24, "Peanuts", 499, 35, false);
    INIT_PRODUCT(25, "Candy", 299, 50, false);
    INIT_PRODUCT(26, "Pepper", 199, 40, false);
    INIT_PRODUCT(27, "Oil", 599, 30, false);
    INIT_PRODUCT(28, "Flour", 349, 35, false);
    INIT_PRODUCT(29, "Tuna", 599, 30, false);
    INIT_PRODUCT(30, "Soup", 399, 25, false);
    INIT_PRODUCT(31, "Beans", 299, 40, false);
    INIT_PRODUCT(32, "Tomato", 179, 60, false);
    INIT_PRODUCT(33, "Potato", 199, 55, false);
    INIT_PRODUCT(34, "Onion", 129, 65, false);
    INIT_PRODUCT(35, "Garlic", 159, 45, false);
    INIT_PRODUCT(36, "Lemon", 129, 40, false);
    INIT_PRODUCT(37, "Orange", 179, 50, false);
    INIT_PRODUCT(38, "Beef", 1299, 20, false);
    INIT_PRODUCT(39, "Chicken", 999, 25, false);
    INIT_PRODUCT(40, "Cake", 899, 15, true);
    INIT_PRODUCT(41, "Deli Meat", 799, 25, true);
    INIT_PRODUCT(42, "Fresh Fish", 1299, 20, true);
    INIT_PRODUCT(43, "Sliced Bread", 399, 30, true);
    INIT_PRODUCT(44, "Cheese Wheel", 1599, 10, true);
    INIT_PRODUCT(45, "Custom Coffee", 699, 35, true);
    INIT_PRODUCT(46, "Watermelon", 599, 20, true);
    INIT_PRODUCT(47, "Fresh Meat", 1099, 15, true);
    INIT_PRODUCT(48, "Salad Mix", 349, 30, true);
    INIT_PRODUCT(49, "Fresh Juice", 899, 25, true);
}

bool try_get_product(int product_id) {
    pthread_mutex_lock(&inventory_mutex);
    
    bool success = false;
    if (product_id < MAX_PRODUCTS && products[product_id].stock > 0) {
        products[product_id].stock--;
        success = true;
    }
    
    pthread_mutex_unlock(&inventory_mutex);
    return success;
}

int get_product_price(int product_id) {
    return products[product_id].price;
}

bool product_needs_assistant(int product_id)
{
    return products[product_id].needs_assistant;
}
