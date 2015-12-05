---
layout: post
title: "Writing your own member attribute handler"
date: 2015-11-30
categories: unity csharp
---

Attributes are the main way C# implements annotations. They are very powerful
tools to simplify adding behaviors. I'm going to explain a simple way of
implementing some functionality attached to member attributes, mainly
focused on Unity3D.

<!-- more -->

When we define an attribute attached to a property or field, we need
to define a class that inherits from `System.Attribute`. Parameters
passed when using the attribute can be named using properties or
unnamed using constructor arguments.

```csharp
public class LocalizeAttribute : System.Attribute
{
  public LocalizeAttribute()
  {
  }
}
```

```csharp
using System;
using System.Reflection;
using System.Collections.Generic;

public interface IMemberAttributeObserver<A> : IDisposable, ICloneable where A : Attribute
{
    bool Supports(object obj, A attr);

    object Apply(object obj, A attr);
}

public class MemberAttributeConfiguration<A> : IDisposable where A : Attribute
{
    IList<IMemberAttributeObserver<A>> _prototypes;
    IDictionary<A,IMemberAttributeObserver<A>> _observers;

    public MemberAttributeConfiguration(List<IMemberAttributeObserver<A>> prototypes=null)
    {
        _observers = new Dictionary<A,IMemberAttributeObserver<A>>();
        if(prototypes == null)
        {
            prototypes = new List<IMemberAttributeObserver<A>>();
        }
        _prototypes = prototypes;
    }

    public virtual void Dispose()
    {
        foreach(var proto in _prototypes)
        {
            proto.Dispose();
        }
        _prototypes.Clear();
        foreach(var kvp in _observers)
        {
            kvp.Value.Dispose();
        }
        _observers.Clear();
    }

    public void AddObserver(IMemberAttributeObserver<A> observer)
    {
        if(observer == null)
        {
            throw new ArgumentNullException("observer");
        }
        if(!_prototypes.Contains(observer))
        {
            _prototypes.Add(observer);
        }
    }

    object Apply(object prop, A attr)
    {
        IMemberAttributeObserver<A> observer = null;
        if(!_observers.TryGetValue(attr, out observer))
        {
            foreach(var proto in _prototypes)
            {
                if(proto.Supports(prop, attr))
                {
                    observer = (IMemberAttributeObserver<A>)proto.Clone();
                    _observers.Add(attr, observer);
                    break;
                }
            }
        }
        if(observer != null)
        {
            return observer.Apply(prop, attr);
        }
        throw new InvalidOperationException(
            string.Format("Could not find any way to manage object of type '{0}'.", prop.GetType().FullName));
    }

    const BindingFlags MemberBindingFlags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly;

    public void Apply(object obj)
    {
        if(obj == null)
        {
            return;
        }
        var type = obj.GetType();
        foreach(var prop in type.GetProperties(MemberBindingFlags))
        {
            foreach(var attrObj in prop.GetCustomAttributes(typeof(A), true))
            {
                var attr = (A)attrObj;
                var val = prop.GetValue(obj, null);
                val = Apply(val, attr);
                prop.SetValue(obj, val, null);
            }
        }
        foreach(var field in type.GetFields(MemberBindingFlags))
        {
            foreach(var attrObj in field.GetCustomAttributes(typeof(A), true))
            {
                var attr = (A)attrObj;
                var val = field.GetValue(obj);
                val = Apply(val, attr);
                field.SetValue(obj, val);
            }
        }
    }
}
```

```csharp
public abstract class BaseMemberAttributeObserver<T,A> : IMemberAttributeObserver<A> where T : class where A : Attribute
{
    public bool Supports(object obj, A attr)
    {
        return obj as T != null;
    }

    public object Apply(object obj, A attr)
    {
        var tobj = obj as T;
        if(tobj == null)
        {
            throw new InvalidOperationException(
                string.Format("Argument needs to be of type {0}", typeof(T).FullName));
        }
        return ApplyType(tobj, attr);
    }

    public virtual void Dispose()
    {
    }

    public abstract object Clone();

    protected abstract T ApplyType(T obj, A attr);

}
```
