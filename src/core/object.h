/**
 * @file    object.h
 * @brief   OOP 基础类型 / Base Object "Class"
 *
 * 所有 OOP 封装类的基类。每个"类"的第一个成员应该是 Object，
 * 这样可以统一管理生命周期和运行时类型识别。
 *
 * ── 用法 ─────────────────────────────────────────────────
 *
 *   typedef struct { Object base; int data; } MyClass;
 *   MyClass obj;
 *   object_ctor(&obj.base, "MyClass");
 *
 *   printf("Type: %s\n", TYPE_NAME(&obj.base)); // "MyClass"
 *
 * ── 设计动机 ────────────────────────────────────────────
 *
 *   在大型 C 项目中，跟踪"这是什么类型"是常见需求。
 *   Object 提供了一个最小化的 RTTI (运行时类型信息) 和
 *   统一的销毁接口。对于只有 64KB Flash 的 STM32，
 *   这个开销极小 (一个 name 指针 + 一个函数指针)。
 *
 * ── 内存开销 ────────────────────────────────────────────
 *   Object: 16 bytes (1 ptr + 1 fn ptr), 仅当使用时才存在
 */

#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Object Object;

struct Object {
    const char *type_name;   /**< 类名 (指向 ROM 中的字面量) */
    void (*destroy)(Object *self); /**< 虚析构函数 */
};

/** @brief 初始化基础对象 (从子类构造函数中调用) */
void object_ctor(Object *self, const char *type_name);

/** @brief 虚析构函数桩 — 子类通过 vtable 重写 */
void object_destroy(Object *self);

/** @brief 从子类指针获取嵌入的 Object* (向上转型) */
#define OBJECT_OF(ptr)  (&((ptr)->base))

/** @brief 获取运行时类型名称 */
#define TYPE_NAME(obj)  ((obj)->type_name)

#ifdef __cplusplus
}
#endif

#endif /* OBJECT_H */
