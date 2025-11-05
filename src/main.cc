#include <napi.h>
#include "WindowManager.h"

std::wstring ToWString(const Napi::Value &value)
{
    if (value.IsString())
    {
        std::string str = value.As<Napi::String>().Utf8Value();
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        if (size > 0)
        {
            std::wstring wstr(size - 1, 0);
            MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
            return wstr;
        }
    }
    return L"";
}

Napi::Value CreateEmbeddedWindow(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    try
    {
        if (info.Length() < 2)
        {
            Napi::TypeError::New(env, "Expected at least 2 arguments").ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!info[0].IsBuffer())
        {
            Napi::TypeError::New(env, "Argument 0 must be a Buffer (window handle)").ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!info[1].IsObject())
        {
            Napi::TypeError::New(env, "Argument 1 must be an options object").ThrowAsJavaScriptException();
            return env.Null();
        }

        Napi::Buffer<void *> wndHandle = info[0].As<Napi::Buffer<void *>>();
        HWND hwndParent = static_cast<HWND>(*reinterpret_cast<void **>(wndHandle.Data()));

        Napi::Object options = info[1].As<Napi::Object>();

        Napi::Maybe<Napi::Value> exePathMaybe = options.Get("exePath");
        if (exePathMaybe.IsNothing())
        {
            Napi::TypeError::New(env, "exePath is required").ThrowAsJavaScriptException();
            return env.Null();
        }
        std::wstring exePath = ToWString(exePathMaybe.Unwrap());
        if (exePath.empty())
        {
            Napi::TypeError::New(env, "exePath is required").ThrowAsJavaScriptException();
            return env.Null();
        }

        std::wstring args;
        Napi::Maybe<Napi::Value> argsMaybe = options.Get("args");
        if (!argsMaybe.IsNothing())
        {
            args = ToWString(argsMaybe.Unwrap());
        }

        int x = 0;
        Napi::Maybe<Napi::Value> xMaybe = options.Get("x");
        if (!xMaybe.IsNothing())
        {
            x = xMaybe.Unwrap().As<Napi::Number>().Int32Value();
        }

        int y = 0;
        Napi::Maybe<Napi::Value> yMaybe = options.Get("y");
        if (!yMaybe.IsNothing())
        {
            y = yMaybe.Unwrap().As<Napi::Number>().Int32Value();
        }

        int width = 800;
        Napi::Maybe<Napi::Value> widthMaybe = options.Get("width");
        if (!widthMaybe.IsNothing())
        {
            width = widthMaybe.Unwrap().As<Napi::Number>().Int32Value();
        }

        int height = 600;
        Napi::Maybe<Napi::Value> heightMaybe = options.Get("height");
        if (!heightMaybe.IsNothing())
        {
            height = heightMaybe.Unwrap().As<Napi::Number>().Int32Value();
        }

        std::string id = WindowManager::Instance().CreateEmbeddedWindow(
            hwndParent, exePath, args, x, y, width, height);

        return Napi::String::New(env, id);
    }
    catch (const std::exception &e)
    {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value UpdateWindow(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    try
    {
        if (info.Length() < 2)
        {
            Napi::TypeError::New(env, "Expected at least 2 arguments").ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!info[0].IsString())
        {
            Napi::TypeError::New(env, "Argument 0 must be a string (window id)").ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!info[1].IsObject())
        {
            Napi::TypeError::New(env, "Argument 1 must be an options object").ThrowAsJavaScriptException();
            return env.Null();
        }

        std::string id = info[0].As<Napi::String>().Utf8Value();
        Napi::Object options = info[1].As<Napi::Object>();

        int x = 0;
        Napi::Maybe<Napi::Value> xMaybe = options.Get("x");
        if (!xMaybe.IsNothing())
        {
            x = xMaybe.Unwrap().As<Napi::Number>().Int32Value();
        }

        int y = 0;
        Napi::Maybe<Napi::Value> yMaybe = options.Get("y");
        if (!yMaybe.IsNothing())
        {
            y = yMaybe.Unwrap().As<Napi::Number>().Int32Value();
        }

        int width = 800;
        Napi::Maybe<Napi::Value> widthMaybe = options.Get("width");
        if (!widthMaybe.IsNothing())
        {
            width = widthMaybe.Unwrap().As<Napi::Number>().Int32Value();
        }

        int height = 600;
        Napi::Maybe<Napi::Value> heightMaybe = options.Get("height");
        if (!heightMaybe.IsNothing())
        {
            height = heightMaybe.Unwrap().As<Napi::Number>().Int32Value();
        }

        bool result = WindowManager::Instance().UpdateWindow(id, x, y, width, height);
        return Napi::Boolean::New(env, result);
    }
    catch (const std::exception &e)
    {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value ShowWindow(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    try
    {
        if (info.Length() < 2)
        {
            Napi::TypeError::New(env, "Expected 2 arguments").ThrowAsJavaScriptException();
            return env.Null();
        }

        std::string id = info[0].As<Napi::String>().Utf8Value();
        bool show = info[1].As<Napi::Boolean>().Value();

        bool result = WindowManager::Instance().ShowWindow(id, show);
        return Napi::Boolean::New(env, result);
    }
    catch (const std::exception &e)
    {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value DestroyWindow(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    try
    {
        if (info.Length() < 1)
        {
            Napi::TypeError::New(env, "Expected 1 argument").ThrowAsJavaScriptException();
            return env.Null();
        }

        std::string id = info[0].As<Napi::String>().Utf8Value();
        bool result = WindowManager::Instance().DestroyWindow(id);
        return Napi::Boolean::New(env, result);
    }
    catch (const std::exception &e)
    {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value GetAllWindowIds(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    try
    {
        auto ids = WindowManager::Instance().GetAllWindowIds();
        Napi::Array result = Napi::Array::New(env, ids.size());

        for (size_t i = 0; i < ids.size(); ++i)
        {
            result[i] = Napi::String::New(env, ids[i]);
        }

        return result;
    }
    catch (const std::exception &e)
    {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Value CleanupAll(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    try
    {
        WindowManager::Instance().CleanupAll();
        return env.Undefined();
    }
    catch (const std::exception &e)
    {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Null();
    }
}

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    // 使用 lambda 函数包装
    exports.Set(
        Napi::String::New(env, "createEmbeddedWindow"),
        Napi::Function::New(env, [](const Napi::CallbackInfo &info)
                            { return CreateEmbeddedWindow(info); }));

    exports.Set(
        Napi::String::New(env, "updateWindow"),
        Napi::Function::New(env, [](const Napi::CallbackInfo &info)
                            { return UpdateWindow(info); }));

    exports.Set(
        Napi::String::New(env, "showWindow"),
        Napi::Function::New(env, [](const Napi::CallbackInfo &info)
                            { return ShowWindow(info); }));

    exports.Set(
        Napi::String::New(env, "destroyWindow"),
        Napi::Function::New(env, [](const Napi::CallbackInfo &info)
                            { return DestroyWindow(info); }));

    exports.Set(
        Napi::String::New(env, "getAllWindowIds"),
        Napi::Function::New(env, [](const Napi::CallbackInfo &info)
                            { return GetAllWindowIds(info); }));

    exports.Set(
        Napi::String::New(env, "cleanupAll"),
        Napi::Function::New(env, [](const Napi::CallbackInfo &info)
                            { return CleanupAll(info); }));

    return exports;
}
NODE_API_MODULE(BrowserWindowTool, Init)