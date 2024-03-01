#include "skiplist.h"
#include <iostream>

using namespace std;

int main(){

    cout << "skiplist test now starts..." << endl;

    //给一个最大的整型值
    SkipList<int, int> skipList(0x7fffffff);

    int length = 10;

    for (int i = 1; i <= length; ++i) {
        skipList.insert(i, i);
    }

    cout << "The number of elements in skiplist is:" << skipList.size() << endl;


    // test range_query
    auto vec = skipList.range_search(1,20);

    cout <<"-------------------------\n";
    for (Node<int,int>  *x : vec)
    {
        cout << "key :" << x->getKey() << " ";
        cout << "value :" << x->getValue() << " \n";
    }
    cout <<"\n";
    cout <<"-------------------------\n";


    //测试查找
    int value = -1;
    int key = 9;
    Node<int, int> *searchResult = skipList.search(key);
    if (searchResult != nullptr) {
        value = searchResult->getValue();
        cout << "search result for key " << key << ":" << value << endl;
    } else {
        cout << "search failure for key " << key << endl;
    }

    //重置value
    value = -1;

    //测试移除,测试不通过
    key = 6;
    cout<<endl<<"start to remove"<<endl;
    bool removeResult = skipList.remove(key, value);
    if (removeResult) {
        cout << "removed node whose key is " << key << " and value is " << value << endl;
    } else {
        cout << "removed failure" << endl;
    }


    return 0;
}
