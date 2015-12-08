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

When defining an attribute attached to a property or field, we need
to create a class that inherits from `System.Attribute`. Parameters
passed when using the attribute can be named using properties or
unnamed using constructor arguments.

```csharp
public class LocalizeAttribute : System.Attribute
{
  public string Key { get; private set; }
  public string DefaultValue { get; private set; }

  public LocalizeAttribute(string key, string defaultValue=null)
  {
    Key = key;
    DefaultValue = defaultValue;
  }
}
```

This attribute can directly be used as an annotation in the following form.

```csharp
public class ExampleBehavior : MonoBehavior
{
  [Localize("example_title", "Example Title")]
  string _title;
}
```

This code would compile, but it wouldn't do anything. Most people assume
that attributes add some functionality to the annotated code parts, but in
reality they just add metadata, the functionality needs to be added elsewhere
by reading the metadata.

First of all, let's talk about modifying the value of an annotated a class member,
a field or a property, that can be of any type. We will define the interface
`IMemberAttributeObserver` that will get called when we find an annotated
member, first we will ask if the observer `Supports` that particular object,
then we will call `Apply` to get the new object value.

```csharp
using System;

public interface IMemberAttributeObserver<A> : IDisposable, ICloneable where A : Attribute
{
    bool Supports(object obj, A attr);

    object Apply(object obj, A attr);
}
```

A typical implementation of this interface will check if the passed object is
of a given type.

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

The second part of the system is a manager class that contains a list
of observer prototypes. This class has a helper method that check for all
the annotated members of a given object.

```csharp
using System;
using System.Reflection;
using System.Collections.Generic;

interface IObjectObserver : IDisposable
{
  void Apply(object behavior);
}

public class MemberAttributesObserver<A> : IObjectObserver where A : Attribute
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

Now we have a system in place to modify the members of an object depending on the
annotated attributes. Where is the best way to do this with all `MonoBehavior`
objects in a scene? In Java the typical way of doing this is the class loader,
but in C# Unity there is no such thing.

Typically `MonoBehavior` objects are created on scene load or by
code when calling `Instantiate` or `AddComponent`. The first event can be detected
with the `OnLevelWasLoaded` message, and the second we can replace with extension methods.

Finding recursively all `MonoBehavior` objects in a scene can be really expensive,
so I recommend only doing this for active root game objects.

```csharp

class MonoBehaviorSceneObserver
{
  List<IObjectObserver> _observers = new List<IObjectObserver>();

  public void AddObserver(IObjectObserver observer)
  {
    if(!_observers.Contains(observer))
    {
      _observers.Add(observer);
    }
  }

  public void Apply(MonoBehavior behavior)
  {
    foreach(var observer in _observers)
    {
      observer.Apply(behavior);
    }
  }

  public void Apply(GameObject go)
  {
    foreach(var comp in go.GetComponents())
    {
      Apply(comp);
    }
  }

  void Awake()
  {
    DontDestroyOnLoad(transform.gameObject);
  }

  void OnLevelWasLoaded()
  {
    foreach (var trans in Object.FindObjectsOfType<Transform>())
    {
        if (trans.parent == null)
        {
            Apply(trans.gameObject);
        }
    }
  }
}

static class UnityAnnotationExtensions
{
  static MonoBehaviorSceneObserver SceneObserver
  {
    get
    {
      return Object.FindObjectOfType<MonoBehaviorSceneObserver>();
    }
  }

  public static void ApplyAnnotations(this GameObject go)
  {
    var sceneObserver = SceneObserver;
    if(sceneObserver != null)
    {
      sceneObserver.Apply(go);
    }
  }

  public static void ApplyAnnotations(this MonoBehavior behavior)
  {
    var sceneObserver = SceneObserver;
    if(sceneObserver != null)
    {
      sceneObserver.Apply(behavior);
    }
  }

  public static GameObject InstantiateAnnotated(this GameObject prefab)
  {
    var go = Instantiate(prefab);
    go.ApplyAnnotations();
    return go;
  }

  public static T AddComponentAnnotated(this GameObject go)
  {
    var behavior = AddComponent<T>();
    behavior.ApplyAnnotations();
    return behavior;
  }
}
```

With this code now we just have to remember to add a game object with the
`MonoBehaviorSceneObserver` component to the scene, and use `InstantiateAnnotated`
and `AddComponentAnnotated` if we whant the annotations to take effect.
